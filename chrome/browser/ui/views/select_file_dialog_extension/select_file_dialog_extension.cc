// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_file_dialog_extension/select_file_dialog_extension.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/platform_apps/app_window_registry_util.h"
#include "chrome/browser/ash/extensions/file_manager/select_file_dialog_extension_user_data.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/select_file_dialog_util.h"
#include "chrome/browser/ash/file_manager/url_util.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/login_web_dialog.h"
#include "chrome/browser/ui/ash/login/webui_login_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chromeos/ui/base/window_properties.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "ui/aura/window.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_palette.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/views/widget/widget.h"

using extensions::AppWindow;

namespace {

const int kFileManagerWidth = 972;  // pixels
const int kFileManagerHeight = 640;  // pixels
const int kFileManagerMinimumWidth = 640;  // pixels
const int kFileManagerMinimumHeight = 240;  // pixels

constexpr char kFakeEntryURLScheme[] = "fake-entry://";
constexpr char kFakeEntryPath[] = "/.fake-entry";

// Holds references to file manager dialogs that have callbacks pending
// to their listeners.
class PendingDialog {
 public:
  static PendingDialog* GetInstance();
  void Add(SelectFileDialogExtension::RoutingID id,
           scoped_refptr<SelectFileDialogExtension> dialog);
  void Remove(SelectFileDialogExtension::RoutingID id);
  scoped_refptr<SelectFileDialogExtension> Find(
      SelectFileDialogExtension::RoutingID id);

 private:
  friend struct base::DefaultSingletonTraits<PendingDialog>;
  using Map = std::map<SelectFileDialogExtension::RoutingID,
                       scoped_refptr<SelectFileDialogExtension>>;
  Map map_;
};

// static
PendingDialog* PendingDialog::GetInstance() {
  static base::NoDestructor<PendingDialog> instance;
  return instance.get();
}

void PendingDialog::Add(SelectFileDialogExtension::RoutingID id,
                        scoped_refptr<SelectFileDialogExtension> dialog) {
  DCHECK(dialog.get());
  if (map_.find(id) == map_.end())
    map_.insert(std::make_pair(id, dialog));
  else
    DLOG(WARNING) << "Duplicate pending dialog " << id;
}

void PendingDialog::Remove(SelectFileDialogExtension::RoutingID id) {
  map_.erase(id);
}

scoped_refptr<SelectFileDialogExtension> PendingDialog::Find(
    SelectFileDialogExtension::RoutingID id) {
  Map::const_iterator it = map_.find(id);
  if (it == map_.end())
    return nullptr;
  return it->second;
}

// Return the Chrome OS WebUI login WebContents, if applicable.
content::WebContents* GetLoginWebContents() {
  auto* host = ash::LoginDisplayHost::default_host();
  return host ? host->GetOobeWebContents() : nullptr;
}

// Given |owner_window| finds corresponding |base_window|, it's associated
// |web_contents| and |profile|.
void FindRuntimeContext(gfx::NativeWindow owner_window,
                        ui::BaseWindow** base_window,
                        content::WebContents** web_contents) {
  *base_window = nullptr;
  *web_contents = nullptr;
  // To get the base_window and web contents, either a Browser or AppWindow is
  // needed.
  Browser* owner_browser = nullptr;
  AppWindow* app_window = nullptr;

  // If owner_window is supplied, use that to find a browser or a app window.
  if (owner_window) {
    owner_browser = chrome::FindBrowserWithWindow(owner_window);
    if (!owner_browser) {
      // If an owner_window was supplied but we couldn't find a browser, this
      // could be for a app window.
      app_window =
          AppWindowRegistryUtil::GetAppWindowForNativeWindowAnyProfile(
              owner_window);
    }
  }

  if (app_window) {
    *base_window = app_window->GetBaseWindow();
    *web_contents = app_window->web_contents();
  } else {
    // If the owning window is still unknown, this could be a background page or
    // and extension popup. Use the last active browser.
    if (!owner_browser)
      owner_browser = chrome::FindLastActive();
    if (owner_browser) {
      *base_window = owner_browser->window();
      *web_contents = owner_browser->tab_strip_model()->GetActiveWebContents();
    }
  }

  // In ChromeOS kiosk launch mode, we can still show file picker for
  // certificate manager dialog. There are no browser or webapp window
  // instances present in this case.
  if (IsRunningInForcedAppMode() && !(*web_contents)) {
    *web_contents = ash::LoginWebDialog::GetCurrentWebContents();
  }

  // Check for a WebContents used for the Chrome OS WebUI login flow.
  if (!*web_contents)
    *web_contents = GetLoginWebContents();
}

SelectFileDialogExtension::RoutingID GetRoutingID(
    content::WebContents* web_contents,
    const SelectFileDialogExtension::Owner& owner) {
  if (owner.android_task_id.has_value())
    return base::StringPrintf("android.%d", *owner.android_task_id);

  // Lacros ids are already prefixed with "lacros".
  if (owner.lacros_window_id.has_value())
    return *owner.lacros_window_id;

  if (web_contents) {
    return base::StringPrintf(
        "web.%d",
        web_contents->GetPrimaryMainFrame()->GetFrameTreeNodeId().value());
  }
  LOG(ERROR) << "Unable to generate a RoutingID";
  return "";
}

}  // namespace

// A customization of SystemWebDialogDelegate that provides notifications
// to SelectFileDialogExtension about web dialog closing events. Must be outside
// anonymous namespace for the friend declaration to work.
class SystemFilesAppDialogDelegate : public ash::SystemWebDialogDelegate {
 public:
  SystemFilesAppDialogDelegate(base::WeakPtr<SelectFileDialogExtension> parent,
                               const std::string& id,
                               GURL url,
                               std::u16string title)
      : ash::SystemWebDialogDelegate(url, title),
        id_(id),
        parent_(std::move(parent)) {
    RegisterOnDialogClosedCallback(
        base::BindOnce(&SystemFilesAppDialogDelegate::OnDialogClosing,
                       base::Unretained(this)));
  }
  ~SystemFilesAppDialogDelegate() override = default;

  void SetModal(bool modal) {
    set_dialog_modal_type(modal ? ui::mojom::ModalType::kWindow
                                : ui::mojom::ModalType::kNone);
  }

  FrameKind GetWebDialogFrameKind() const override {
    // The default is kDialog, however it doesn't allow to customize the title
    // color and to make the dialog movable and re-sizable.
    return FrameKind::kNonClient;
  }

  void GetMinimumDialogSize(gfx::Size* size) const override {
    size->set_width(kFileManagerMinimumWidth);
    size->set_height(kFileManagerMinimumHeight);
  }

  void GetDialogSize(gfx::Size* size) const override {
    *size = SystemWebDialogDelegate::ComputeDialogSizeForInternalScreen(
        {kFileManagerWidth, kFileManagerHeight});
  }

  void OnDialogShown(content::WebUI* webui) override {
    if (parent_) {
      parent_->OnSystemDialogShown(webui->GetWebContents(), id_);
    }
    ash::SystemWebDialogDelegate::OnDialogShown(webui);
  }

  void OnDialogClosing(const std::string& ret_val) {
    if (parent_) {
      parent_->OnSystemDialogWillClose();
    }
  }

 private:
  // The routing ID. We store it so that we can call back into the
  // SelectFileDialog to inform it about contents::WebContents and
  // the ID associated with it.
  const std::string id_;

  // The parent of this delegate.
  base::WeakPtr<SelectFileDialogExtension> parent_;
};

/////////////////////////////////////////////////////////////////////////////

SelectFileDialogExtension::Owner::Owner() = default;
SelectFileDialogExtension::Owner::~Owner() = default;
SelectFileDialogExtension::Owner::Owner(
    const SelectFileDialogExtension::Owner&) = default;
SelectFileDialogExtension::Owner& SelectFileDialogExtension::Owner::operator=(
    const SelectFileDialogExtension::Owner&) = default;
SelectFileDialogExtension::Owner::Owner(SelectFileDialogExtension::Owner&&) =
    default;
SelectFileDialogExtension::Owner& SelectFileDialogExtension::Owner::operator=(
    SelectFileDialogExtension::Owner&&) = default;

// static
SelectFileDialogExtension* SelectFileDialogExtension::Create(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return new SelectFileDialogExtension(listener, std::move(policy));
}

SelectFileDialogExtension::SelectFileDialogExtension(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : SelectFileDialog(listener, std::move(policy)),
      system_files_app_web_contents_(nullptr) {}

SelectFileDialogExtension::~SelectFileDialogExtension() = default;

bool SelectFileDialogExtension::IsRunning(
    gfx::NativeWindow owner_window) const {
  return owner_.window == owner_window;
}

void SelectFileDialogExtension::ListenerDestroyed() {
  listener_ = nullptr;
  PendingDialog::GetInstance()->Remove(routing_id_);
}

void SelectFileDialogExtension::OnSystemDialogShown(
    content::WebContents* web_contents,
    const std::string& id) {
  system_files_app_web_contents_ = web_contents;
  SelectFileDialogExtensionUserData::SetDialogDataForWebContents(
      web_contents, id, type_, owner_.dialog_caller);
}

void SelectFileDialogExtension::OnSystemDialogWillClose() {
  profile_ = nullptr;
  auto dialog_caller = owner_.dialog_caller;
  owner_ = {};
  system_files_app_web_contents_ = nullptr;
  PendingDialog::GetInstance()->Remove(routing_id_);
  // Actually invoke the appropriate callback on our listener.
  ApplyPolicyAndNotifyListener(std::move(dialog_caller));
}

// static
void SelectFileDialogExtension::OnFileSelected(
    RoutingID routing_id,
    const ui::SelectedFileInfo& file,
    int index) {
  scoped_refptr<SelectFileDialogExtension> dialog =
      PendingDialog::GetInstance()->Find(routing_id);
  if (!dialog.get())
    return;
  dialog->selection_type_ = SINGLE_FILE;
  dialog->selection_files_.clear();
  dialog->selection_files_.push_back(file);
  dialog->selection_index_ = index;
}

// static
void SelectFileDialogExtension::OnMultiFilesSelected(
    RoutingID routing_id,
    const std::vector<ui::SelectedFileInfo>& files) {
  scoped_refptr<SelectFileDialogExtension> dialog =
      PendingDialog::GetInstance()->Find(routing_id);
  if (!dialog.get())
    return;
  dialog->selection_type_ = MULTIPLE_FILES;
  dialog->selection_files_ = files;
  dialog->selection_index_ = 0;
}

// static
void SelectFileDialogExtension::OnFileSelectionCanceled(RoutingID routing_id) {
  scoped_refptr<SelectFileDialogExtension> dialog =
      PendingDialog::GetInstance()->Find(routing_id);
  if (!dialog.get())
    return;
  dialog->selection_type_ = CANCEL;
  dialog->selection_files_.clear();
  dialog->selection_index_ = 0;
}

content::RenderFrameHost* SelectFileDialogExtension::GetPrimaryMainFrame() {
  if (system_files_app_web_contents_)
    return system_files_app_web_contents_->GetPrimaryMainFrame();
  return nullptr;
}

GURL SelectFileDialogExtension::MakeDialogURL(
    Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const std::string& search_query,
    bool show_android_picker_apps,
    std::vector<std::string> volume_filter,
    Profile* profile) {
  base::FilePath relative_path;
  if (base::FilePath(kFakeEntryPath)
          .AppendRelativePath(default_path, &relative_path)) {
    GURL selection_url = GURL(kFakeEntryURLScheme + relative_path.value());
    GURL current_directory_url =
        GURL(kFakeEntryURLScheme + relative_path.DirName().value());
    return file_manager::util::GetFileManagerMainPageUrlWithParams(
        type, title, current_directory_url, selection_url, /*target_name=*/"",
        file_types, file_type_index, search_query, show_android_picker_apps,
        std::move(volume_filter));
  }

  base::FilePath download_default_path(
      DownloadPrefs::FromBrowserContext(profile)->DownloadPath());
  base::FilePath selection_path =
      default_path.IsAbsolute()
          ? default_path
          : download_default_path.Append(default_path.BaseName());
  base::FilePath fallback_path = profile->last_selected_directory().empty()
                                     ? download_default_path
                                     : profile->last_selected_directory();

  // Convert the above absolute paths to file system URLs.
  GURL selection_url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, selection_path, file_manager::util::GetFileManagerURL(),
          &selection_url)) {
    // Due to the current design, an invalid temporal cache file path may passed
    // as |default_path| (crbug.com/178013 #9-#11). In such a case, we use the
    // last selected directory as a workaround. Real fix is tracked at
    // crbug.com/110119.
    base::FilePath base_name = default_path.BaseName();
    if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
            profile,
            // If the base_name is absolute (happens for default_path '/') it's
            // not usable in Append.
            base_name.IsAbsolute() ? fallback_path
                                   : fallback_path.Append(base_name),
            file_manager::util::GetFileManagerURL(), &selection_url)) {
      DVLOG(1) << "Unable to resolve the selection URL.";
    }
  }

  GURL current_directory_url;
  base::FilePath current_directory_path = selection_path.DirName();
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, current_directory_path,
          file_manager::util::GetFileManagerURL(), &current_directory_url)) {
    // Fallback if necessary, see the comment above.
    if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
            profile, fallback_path, file_manager::util::GetFileManagerURL(),
            &current_directory_url)) {
      DVLOG(1) << "Unable to resolve the current directory URL for: "
               << fallback_path.value();
    }
  }

  return file_manager::util::GetFileManagerMainPageUrlWithParams(
      type, title, current_directory_url, selection_url,
      default_path.BaseName().value(), file_types, file_type_index,
      search_query, show_android_picker_apps, std::move(volume_filter));
}

void SelectFileDialogExtension::SelectFileWithFileManagerParams(
    Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const Owner& owner,
    const std::string& search_query,
    bool show_android_picker_apps,
    bool use_media_store_filter) {
  if (owner_.window) {
    LOG(ERROR) << "File dialog already in use!";
    return;
  }

  // The base window to associate the dialog with.
  ui::BaseWindow* base_window = nullptr;

  // The web contents to associate the dialog with.
  content::WebContents* web_contents = nullptr;

  // The folder selection dialog created for capture mode should never be
  // parented to a browser window (if one exists). https://crbug.com/1258842.
  const bool is_for_capture_mode =
      owner.window &&
      owner.window->GetId() ==
          ash::kShellWindowId_CaptureModeFolderSelectionDialogOwner;

  const bool skip_finding_browser = is_for_capture_mode ||
                                    owner.android_task_id.has_value() ||
                                    owner.lacros_window_id.has_value();

  can_resize_ =
      !display::Screen::GetScreen()->InTabletMode() && !is_for_capture_mode;

  // Obtain BaseWindow and WebContents if the owner window is browser.
  if (!skip_finding_browser)
    FindRuntimeContext(owner.window, &base_window, &web_contents);

  if (web_contents)
    profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // Handle the cases where |web_contents| is not available or |web_contents| is
  // associated with Default profile.
  if (!web_contents || ash::ProfileHelper::IsSigninProfile(profile_))
    profile_ = ProfileManager::GetActiveUserProfile();

  DCHECK(profile_);

  // Check if we have another dialog opened for the contents. It's unlikely, but
  // possible. In such situation, discard this request.
  RoutingID routing_id = GetRoutingID(web_contents, owner);
  if (PendingExists(routing_id))
    return;

  std::vector<std::string> volume_filter;
  if (owner.is_lacros) {
    // SelectFileAsh (Lacros) is opening the dialog: only show fusebox volumes
    // in File Manager UI to return real file descriptors to SelectFileAsh.
    // TODO(crbug.com/369851375): Delete this; Lacros has sunset.
    volume_filter.push_back("fusebox-only");
  } else if (use_media_store_filter) {
    // ArcSelectFile is opening the dialog: add 'media-store-files-only' filter
    // to only show volumes in File Manager UI that are indexed by the Android
    // MediaStore.
    volume_filter.push_back("media-store-files-only");
  }

  GURL file_manager_url = SelectFileDialogExtension::MakeDialogURL(
      type, title, default_path, file_types, file_type_index, search_query,
      show_android_picker_apps, std::move(volume_filter), profile_);

  has_multiple_file_type_choices_ =
      !file_types || (file_types->extensions.size() > 1);

  std::u16string dialog_title =
      !title.empty() ? title
                     : file_manager::util::GetSelectFileDialogTitle(type);
  gfx::NativeWindow parent_window =
      base_window ? base_window->GetNativeWindow() : owner.window.get();

  owner_ = owner;
  type_ = type;

  // The delegate deletes itself in WebDialogDelegate::OnDialogClosed().
  auto* dialog_delegate = new SystemFilesAppDialogDelegate(
      weak_factory_.GetWeakPtr(), routing_id, file_manager_url, dialog_title);
  dialog_delegate->SetModal(owner.window != nullptr);
  dialog_delegate->set_can_resize(can_resize_);
  dialog_delegate->ShowSystemDialogForBrowserContext(profile_, parent_window);

  // Connect our listener to FileDialogFunction's per-tab callbacks.
  AddPending(routing_id);

  routing_id_ = routing_id;
}

void SelectFileDialogExtension::SelectFileImpl(
    Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owner_window,
    const GURL* caller) {
  // |default_extension| is ignored.
  Owner owner;
  owner.window = owner_window;
  if (caller && caller->is_valid()) {
    owner.dialog_caller.emplace(*caller);
  }
  SelectFileWithFileManagerParams(type, title, default_path, file_types,
                                  file_type_index, owner,
                                  /*search_query=*/"",
                                  /*show_android_picker_apps=*/false);
}

bool SelectFileDialogExtension::HasMultipleFileTypeChoicesImpl() {
  return has_multiple_file_type_choices_;
}

bool SelectFileDialogExtension::IsResizeable() const {
  return can_resize_;
}

void SelectFileDialogExtension::ApplyPolicyAndNotifyListener(
    std::optional<policy::DlpFileDestination> dialog_caller) {
  if (!listener_)
    return;

  // The selected files are passed by reference to the listener. Ensure they
  // outlive the dialog if it is immediately deleted by the listener.
  std::vector<ui::SelectedFileInfo> selection_files =
      std::move(selection_files_);
  selection_files_.clear();

  if (!dialog_caller.has_value() || selection_files.empty()) {
    NotifyListener(std::move(selection_files));
    return;
  }

  if (auto* files_controller =
          policy::DlpFilesControllerAsh::GetForPrimaryProfile();
      files_controller && type_ == Type::SELECT_SAVEAS_FILE) {
    files_controller->CheckIfDownloadAllowed(
        dialog_caller.value(),
        // TODO(crbug.com/1385687): Handle selection_files.size() > 1.
        selection_files[0].local_path.empty() ? selection_files[0].file_path
                                              : selection_files[0].local_path,
        base::BindOnce(
            [](base::WeakPtr<SelectFileDialogExtension> weak_ptr,
               std::vector<ui::SelectedFileInfo> selection_files,
               bool is_allowed) {
              if (!is_allowed)
                weak_ptr->selection_type_ = SelectionType::CANCEL;
              weak_ptr->NotifyListener(std::move(selection_files));
            },
            weak_factory_.GetWeakPtr(), std::move(selection_files)));
    return;
  } else if (files_controller) {
    files_controller->FilterDisallowedUploads(
        std::move(selection_files), dialog_caller.value(),
        base::BindOnce(
            [](base::WeakPtr<SelectFileDialogExtension> weak_ptr,
               std::vector<ui::SelectedFileInfo> allowed_files) {
              if (allowed_files.empty())
                weak_ptr->selection_type_ = SelectionType::CANCEL;
              weak_ptr->NotifyListener(std::move(allowed_files));
            },
            weak_factory_.GetWeakPtr()));
    return;
  }
  NotifyListener(std::move(selection_files));
}

void SelectFileDialogExtension::NotifyListener(
    std::vector<ui::SelectedFileInfo> selection_files) {
  if (!listener_)
    return;
  switch (selection_type_) {
    case CANCEL:
      listener_->FileSelectionCanceled();
      break;
    case SINGLE_FILE:
      listener_->FileSelected(selection_files[0], selection_index_);
      break;
    case MULTIPLE_FILES:
      listener_->MultiFilesSelected(selection_files);
      break;
    default:
      NOTREACHED();
  }
}

void SelectFileDialogExtension::AddPending(RoutingID routing_id) {
  PendingDialog::GetInstance()->Add(routing_id, this);
}

// static
bool SelectFileDialogExtension::PendingExists(RoutingID routing_id) {
  return PendingDialog::GetInstance()->Find(routing_id).get() != nullptr;
}
