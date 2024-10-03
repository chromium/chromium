// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"

#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/file_util_icu.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/cloud_devices/common/printer_description.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/printing_context.h"
#include "printing/units.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/common/printing/printer_capabilities_mac.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "components/drive/file_system_core_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "components/user_manager/user.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/common/chrome_paths_lacros.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace printing {

namespace {

constexpr base::FilePath::CharType kPdfExtension[] = FILE_PATH_LITERAL("pdf");

class PrintingContextDelegate : public PrintingContext::Delegate {
 public:
  // PrintingContext::Delegate methods.
  gfx::NativeView GetParentView() override { return gfx::NativeView(); }
  std::string GetAppLocale() override {
    return g_browser_process->GetApplicationLocale();
  }
};

const AccountId& GetAccountId(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const auto* user = ash::ProfileHelper::Get()->GetUserByProfile(profile);
  return user ? user->GetAccountId() : EmptyAccountId();
#else
  return EmptyAccountId();
#endif
}

gfx::Size GetDefaultPdfMediaSizeMicrons() {
  PrintingContextDelegate delegate;
  // The `PrintingContext` for "Save as PDF" does not need to make system
  // printing calls, it just relies on localization plus hardcoded defaults
  // from `PrintingContext::UsePdfSettings()`.  This means that OOP support
  // is unnecessary in this case.
  auto printing_context(PrintingContext::Create(
      &delegate, PrintingContext::ProcessBehavior::kOopDisabled));
  printing_context->UsePdfSettings();
  gfx::Size pdf_media_size = printing_context->GetPdfPaperSizeDeviceUnits();
  float device_microns_per_device_unit =
      static_cast<float>(kMicronsPerInch) /
      printing_context->settings().device_units_per_inch();
  return gfx::Size(pdf_media_size.width() * device_microns_per_device_unit,
                   pdf_media_size.height() * device_microns_per_device_unit);
}

base::Value::Dict GetPdfCapabilities(
    const std::string& locale,
    PrinterSemanticCapsAndDefaults::Papers custom_papers) {
  using cloud_devices::printer::MediaSize;

  cloud_devices::CloudDeviceDescription description;
  cloud_devices::printer::OrientationCapability orientation;
  orientation.AddOption(cloud_devices::printer::OrientationType::PORTRAIT);
  orientation.AddOption(cloud_devices::printer::OrientationType::LANDSCAPE);
  orientation.AddDefaultOption(
      cloud_devices::printer::OrientationType::AUTO_ORIENTATION, true);
  orientation.SaveTo(&description);

  cloud_devices::printer::ColorCapability color;
  {
    cloud_devices::printer::Color standard_color(
        cloud_devices::printer::ColorType::STANDARD_COLOR);
    standard_color.vendor_id =
        base::NumberToString(static_cast<int>(mojom::ColorModel::kColor));
    color.AddDefaultOption(standard_color, true);
  }
  color.SaveTo(&description);

  static constexpr MediaSize kPdfMedia[] = {
      MediaSize::ISO_A0,   MediaSize::ISO_A1,    MediaSize::ISO_A2,
      MediaSize::ISO_A3,   MediaSize::ISO_A4,    MediaSize::ISO_A5,
      MediaSize::NA_LEGAL, MediaSize::NA_LETTER, MediaSize::NA_LEDGER};
  cloud_devices::printer::Media default_media =
      cloud_devices::printer::MediaBuilder()
          .WithSizeAndDefaultPrintableArea(GetDefaultPdfMediaSizeMicrons())
          .WithNameMaybeBasedOnSize(/*custom_display_name=*/"",
                                    /*vendor_id=*/"")
          .Build();
  if (!base::Contains(kPdfMedia, default_media.size_name)) {
    default_media =
        cloud_devices::printer::MediaBuilder()
            .WithStandardName(locale == "en-US" ? MediaSize::NA_LETTER
                                                : MediaSize::ISO_A4)
            .WithSizeAndPrintableAreaBasedOnStandardName()
            .Build();
  }
  cloud_devices::printer::MediaCapability media;
  for (const auto& pdf_media : kPdfMedia) {
    cloud_devices::printer::Media media_option =
        cloud_devices::printer::MediaBuilder()
            .WithStandardName(pdf_media)
            .WithSizeAndPrintableAreaBasedOnStandardName()
            .Build();
    media.AddDefaultOption(media_option,
                           default_media.size_name == media_option.size_name);
  }
  for (const PrinterSemanticCapsAndDefaults::Paper& paper : custom_papers) {
    media.AddOption(cloud_devices::printer::MediaBuilder()
                        .WithCustomName(paper.display_name(), paper.vendor_id())
                        .WithSizeAndPrintableArea(paper.size_um(),
                                                  paper.printable_area_um())
                        .WithBorderlessVariant(paper.has_borderless_variant())
                        .Build());
  }
  media.SaveTo(&description);

  // DPI value should match PrintingContext::UsePdfSettings().
  cloud_devices::printer::DpiCapability dpi;
  dpi.AddDefaultOption(
      cloud_devices::printer::Dpi(kDefaultPdfDpi, kDefaultPdfDpi),
      /*is_default=*/true);
  dpi.SaveTo(&description);

  base::Value capabilities = std::move(description).ToValue();
  return std::move(capabilities).TakeDict();
}

// Callback that stores a PDF file on disk.
void PrintToPdfCallback(scoped_refptr<base::RefCountedMemory> data,
                        const base::FilePath& path) {
  base::WriteFile(path, *data);
}

// Callback that runs after `PrintToPdfCallback()` returns.
void OnPdfPrintedCallback(const AccountId& account_id,
                          const base::FilePath& path,
                          base::OnceClosure pdf_file_saved_closure) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* profile =
      ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (profile) {
    ash::HoldingSpaceKeyedService* holding_space_keyed_service =
        ash::HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
            profile);
    if (holding_space_keyed_service) {
      holding_space_keyed_service->AddItemOfType(
          ash::HoldingSpaceItem::Type::kPrintedPdf, path);
    }
  }
#endif
  if (!pdf_file_saved_closure.is_null())
    std::move(pdf_file_saved_closure).Run();
}

base::FilePath CreateDirectoryIfNotExists(const base::FilePath& path) {
  if (!base::DirectoryExists(path)) {
    base::CreateDirectory(path);
  }
  return path;
}

base::FilePath SelectSaveDirectory(const base::FilePath& path,
                                   const base::FilePath& default_path) {
  if (base::DirectoryExists(path))
    return path;
  return CreateDirectoryIfNotExists(default_path);
}

void ConstructCapabilitiesAndCompleteCallback(
    const std::string& destination_id,
    PdfPrinterHandler::GetCapabilityCallback callback,
    PrinterSemanticCapsAndDefaults::Papers custom_papers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Value::Dict printer_info;
  printer_info.Set(kSettingDeviceName, destination_id);
  printer_info.Set(kSettingCapabilities,
                   GetPdfCapabilities(g_browser_process->GetApplicationLocale(),
                                      custom_papers));
  std::move(callback).Run(std::move(printer_info));
}

}  // namespace

PdfPrinterHandler::PdfPrinterHandler(
    Profile* profile,
    content::WebContents* preview_web_contents,
    PrintPreviewStickySettings* sticky_settings)
    : preview_web_contents_(preview_web_contents),
      profile_(profile),
      sticky_settings_(sticky_settings) {}

PdfPrinterHandler::~PdfPrinterHandler() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void PdfPrinterHandler::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void PdfPrinterHandler::StartGetPrinters(
    AddedPrintersCallback added_printers_callback,
    GetPrintersDoneCallback done_callback) {
  NOTREACHED_IN_MIGRATION();
}

void PdfPrinterHandler::StartGetCapability(const std::string& destination_id,
                                           GetCapabilityCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(IS_MAC)
  // Read the Mac custom paper sizes on a separate thread.
  // USER_VISIBLE because the result is displayed in the print preview dialog.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetMacCustomPaperSizes),
      base::BindOnce(&ConstructCapabilitiesAndCompleteCallback, destination_id,
                     std::move(callback)));
#else
  ConstructCapabilitiesAndCompleteCallback(
      destination_id, std::move(callback),
      PrinterSemanticCapsAndDefaults::Papers());
#endif
}

void PdfPrinterHandler::StartPrint(
    const std::u16string& job_title,
    base::Value::Dict settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    PrintCallback callback) {
  print_data_ = print_data;
  if (!print_to_pdf_path_.empty()) {
    // User has already selected a path, no need to show the dialog again.
    PostPrintToPdfTask();
    return;
  }

  if (select_file_dialog_ &&
      select_file_dialog_->IsRunning(
          platform_util::GetTopLevel(preview_web_contents_->GetNativeView()))) {
    // Dialog is already showing.
    return;
  }

  DCHECK(!print_callback_);
  print_callback_ = std::move(callback);

  auto* dialog_controller = PrintPreviewDialogController::GetInstance();
  CHECK(dialog_controller);
  content::WebContents* initiator =
      dialog_controller->GetInitiator(preview_web_contents_);

  GURL initiator_url;
  bool is_savable = false;
  if (initiator) {
    initiator_url = initiator->GetLastCommittedURL();
    is_savable = initiator->IsSavable();
  }
  base::FilePath path = GetFileName(initiator_url, job_title, is_savable);

  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  bool prompt_user = !cmdline->HasSwitch(switches::kKioskModePrinting);
#if BUILDFLAG(IS_CHROMEOS)
  use_drive_mount_ =
      settings.FindBool(kSettingPrintToGoogleDrive).value_or(false);
#endif

  SelectFile(path, initiator, prompt_user);
}

void PdfPrinterHandler::FileSelected(const ui::SelectedFileInfo& file,
                                     int /* index */) {
  // Update downloads location and save sticky settings.
  DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(profile_);
  download_prefs->SetSaveFilePath(file.path().DirName());
  sticky_settings_->SaveInPrefs(profile_->GetPrefs());
  print_to_pdf_path_ = file.path();
  select_file_dialog_.reset();
  PostPrintToPdfTask();
}

void PdfPrinterHandler::FileSelectionCanceled() {
  std::move(print_callback_).Run(base::Value("PDFPrintCanceled"));
  select_file_dialog_.reset();
}

void PdfPrinterHandler::SetPdfSavedClosureForTesting(
    base::OnceClosure closure) {
  pdf_file_saved_closure_ = std::move(closure);
}

void PdfPrinterHandler::SetPrintToPdfPathForTesting(
    const base::FilePath& path) {
  print_to_pdf_path_ = path;
}

// static
base::FilePath PdfPrinterHandler::GetFileNameForPrintJobTitle(
    const std::u16string& job_title) {
  DCHECK(!job_title.empty());
#if BUILDFLAG(IS_WIN)
  base::FilePath::StringType print_job_title(base::AsWString(job_title));
#elif BUILDFLAG(IS_POSIX)
  base::FilePath::StringType print_job_title = base::UTF16ToUTF8(job_title);
#endif

  base::i18n::ReplaceIllegalCharactersInPath(&print_job_title, '_');
  base::FilePath default_filename(print_job_title);
  base::FilePath::StringType ext = default_filename.Extension();
  if (!ext.empty()) {
    ext = ext.substr(1);
    if (ext == kPdfExtension)
      return default_filename;
  }
  return default_filename.AddExtension(kPdfExtension);
}

// static
base::FilePath PdfPrinterHandler::GetFileNameForURL(const GURL& url) {
  DCHECK(url.is_valid());

  // TODO(thestig): This code is based on similar code in SavePackage in
  // content/ that is not exposed via the public content API. Consider looking
  // for a sane way to share the code.
  if (url.SchemeIs(url::kDataScheme)) {
    return base::FilePath::FromUTF8Unsafe("dataurl").ReplaceExtension(
        kPdfExtension);
  }

  base::FilePath name =
      net::GenerateFileName(url, std::string(), std::string(), std::string(),
                            std::string(), std::string());

  // If host is used as file name, try to decode punycode.
  if (name.AsUTF8Unsafe() == url.host()) {
    name = base::FilePath::FromUTF16Unsafe(
        url_formatter::IDNToUnicode(url.host()));
  }
  if (name.AsUTF8Unsafe() == url.host())
    return name.AddExtension(kPdfExtension);
  return name.ReplaceExtension(kPdfExtension);
}

// static
base::FilePath PdfPrinterHandler::GetFileName(const GURL& url,
                                              const std::u16string& job_title,
                                              bool is_savable) {
  if (is_savable) {
    bool title_is_url =
        job_title.empty() || url_formatter::FormatUrl(url) == job_title;
    return title_is_url ? GetFileNameForURL(url)
                        : GetFileNameForPrintJobTitle(job_title);
  }
  base::FilePath name = net::GenerateFileName(
      url, std::string(), std::string(), std::string(), std::string(),
      l10n_util::GetStringUTF8(IDS_DEFAULT_DOWNLOAD_FILENAME));
  return name.ReplaceExtension(kPdfExtension);
}

void PdfPrinterHandler::SelectFile(const base::FilePath& default_filename,
                                   content::WebContents* initiator,
                                   bool prompt_user) {
  // Handle case where user expects to be prompted but policy disallows file
  // selection. Call CanOpenSelectFileDialog() to notify user and early return.
  if (prompt_user) {
    ChromeSelectFilePolicy policy(initiator);
    if (!policy.CanOpenSelectFileDialog()) {
      policy.SelectFileDenied();
      std::move(print_callback_).Run(base::Value("PDFPrintCannotSelect"));
      return;
    }
  }

  sticky_settings_->SaveInPrefs(profile_->GetPrefs());

  OnSaveLocationReady(default_filename, prompt_user, GetSaveLocation());
}

void PdfPrinterHandler::OnSaveLocationReady(
    const base::FilePath& default_filename,
    bool prompt_user,
    const base::FilePath& path) {
  // Handle the no prompting case. Like the dialog prompt, this function
  // returns and eventually FileSelected() gets called.
  if (!prompt_user) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&base::GetUniquePath, path.Append(default_filename)),
        base::BindOnce(&PdfPrinterHandler::OnGotUniqueFileName,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If the directory is empty there is no reason to create it or use the
  // default location.
  if (path.empty()) {
    OnDirectorySelected(default_filename, path);
    return;
  }

  // Get default download directory. This will be used as a fallback if the
  // save directory does not exist.
  DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(profile_);
  base::FilePath default_path = download_prefs->DownloadPath();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&SelectSaveDirectory, path, default_path),
      base::BindOnce(&PdfPrinterHandler::OnDirectorySelected,
                     weak_ptr_factory_.GetWeakPtr(), default_filename));
}

void PdfPrinterHandler::PostPrintToPdfTask() {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&PrintToPdfCallback, print_data_, print_to_pdf_path_),
      base::BindOnce(&OnPdfPrintedCallback, GetAccountId(profile_),
                     print_to_pdf_path_, std::move(pdf_file_saved_closure_)));

  print_to_pdf_path_.clear();

  if (print_callback_)
    std::move(print_callback_).Run(base::Value());
}

void PdfPrinterHandler::OnGotUniqueFileName(const base::FilePath& path) {
  FileSelected(ui::SelectedFileInfo(path), 0);
}

void PdfPrinterHandler::OnDirectorySelected(const base::FilePath& filename,
                                            const base::FilePath& directory) {
  // Early return if the select file dialog is already active.
  if (select_file_dialog_)
    return;

  base::FilePath path = directory.Append(filename);

  // Prompts the user to select the file.
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(kPdfExtension);
  file_type_info.include_all_files = true;
  // Print Preview requires native paths to write PDF files.
  // Note that Chrome OS save-as dialog has Google Drive as a saving location
  // even when a client requires native paths. In this case, Chrome OS save-as
  // dialog returns native paths to write files and uploads the saved files to
  // Google Drive later.
  file_type_info.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;

  gfx::NativeView owning_window = preview_web_contents_->GetNativeView();
#if defined(USE_AURA)
  if (!owning_window->IsVisible()) {
    auto* dialog_controller = PrintPreviewDialogController::GetInstance();
    CHECK(dialog_controller);
    auto* initiator = dialog_controller->GetInitiator(preview_web_contents_);
    if (initiator) {
      owning_window = initiator->GetNativeView();
    }
  }
#endif

  select_file_dialog_ =
      ui::SelectFileDialog::Create(this, nullptr /*policy already checked*/);
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(), path,
      &file_type_info, 0, kPdfExtension,
      platform_util::GetTopLevel(owning_window), nullptr);
}

base::FilePath PdfPrinterHandler::GetSaveLocation() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  if (use_drive_mount_ && drive_service && drive_service->IsMounted()) {
    return drive_service->GetMountPointPath().Append(
        drive::util::kDriveMyDriveRootDirName);
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  base::FilePath drivefs;
  if (use_drive_mount_ && chrome::GetDriveFsMountPointPath(&drivefs)) {
    return drivefs.Append(drive::util::kDriveMyDriveRootDirName);
  }
#endif
  DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(profile_);
  return download_prefs->SaveFilePath();
}

}  // namespace printing
