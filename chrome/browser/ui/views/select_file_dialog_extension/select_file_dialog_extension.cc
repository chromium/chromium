// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_file_dialog_extension/select_file_dialog_extension.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/multi_user/multi_user_window_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/apps/platform_apps/app_window_registry_util.h"
#include "chrome/browser/ash/extensions/file_manager/select_file_dialog_extension_user_data.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/select_file_dialog_util.h"
#include "chrome/browser/ash/file_manager/url_util.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
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

const int kFileManagerWidth = 972;          // pixels
const int kFileManagerHeight = 640;         // pixels
const int kFileManagerMinimumWidth = 640;   // pixels
const int kFileManagerMinimumHeight = 240;  // pixels

constexpr char kFakeEntryURLScheme[] = "fake-entry://";
constexpr char kFakeEntryPath[] = "/.fake-entry";

// Set on the owner window while a Files app dialog opened for it is showing,
// so that a request to open a second dialog for the same window is rejected.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kHasOpenSelectFileDialogKey, false)

// The window to parent the Files app dialog to, and the profile to run it in.
struct RuntimeContext {
  gfx::NativeWindow parent_window = gfx::NativeWindow();
  raw_ptr<Profile> profile = nullptr;
};

// Returns the fully initialized Profile for `account_id`, or nullptr.
Profile* GetProfileByAccountId(const AccountId& account_id) {
  return Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          account_id));
}

// Derives the RuntimeContext for `owner_window`:
//  - A Browser or extension AppWindow: parent to it, use its profile.
//  - Any other window (e.g. a system web dialog, a login dialog, or an ash
//    widget such as the capture mode folder selection dimmer): parent to it,
//    use the profile of the window's owner user if any, otherwise the active
//    user profile.
//  - No window (e.g. a file picker requested by a VM app, or a download
//    started by an extension service worker): parent to the last active
//    browser, if any, and use its profile.
RuntimeContext GetRuntimeContext(gfx::NativeWindow owner_window) {
  RuntimeContext context;
  if (owner_window) {
    context.parent_window = owner_window;
    if (BrowserWindowInterface* browser =
            GlobalBrowserCollection::GetInstance()->FindBrowserWithWindow(
                owner_window)) {
      context.profile = browser->GetProfile();
    } else if (AppWindow* app_window =
                   AppWindowRegistryUtil::GetAppWindowForNativeWindowAnyProfile(
                       owner_window)) {
      context.profile =
          Profile::FromBrowserContext(app_window->browser_context());
    } else if (!owner_window->GetRootWindow()) {
      // TODO(crbug.com/252339992): Handle the window of a WebContents that is
      // not attached to any widget (e.g. an extension background page or an
      // offscreen document). Such a window is not in any window tree, so it
      // should not be used as the parent, and the profile should come from
      // the WebContents.
    } else if (auto* multi_user_window_manager =
                   ash::MultiUserWindowManager::Get()) {
      const AccountId& account_id =
          multi_user_window_manager->GetWindowOwner(owner_window);
      if (account_id.is_valid()) {
        context.profile = GetProfileByAccountId(account_id);
      }
    }
  } else if (BrowserWindowInterface* browser =
                 GlobalBrowserCollection::GetInstance()
                     ->GetLastActiveBrowser()) {
    // CAUTION: The last active browser may not be accurate. For example, if
    // the dialog is opened on an empty desktop after switching from another
    // user's desktop, the last active browser may belong to the other user.
    context.parent_window = browser->GetWindow()->GetNativeWindow();
    context.profile = browser->GetProfile();
  }
  if (!context.profile) {
    const session_manager::Session* active_session =
        session_manager::SessionManager::Get()->GetActiveSession();
    // The dialog is not expected on the login screen, where there is no user
    // session: the Files app can't run in the sign-in profile.
    DUMP_WILL_BE_CHECK(active_session);
    if (active_session) {
      context.profile = GetProfileByAccountId(active_session->account_id());
    }
  }
  return context;
}

}  // namespace

// A customization of SystemWebDialogDelegate that provides notifications
// to SelectFileDialogExtension about web dialog closing events. Must be outside
// anonymous namespace for the friend declaration to work.
class SystemFilesAppDialogDelegate : public ash::SystemWebDialogDelegate {
 public:
  SystemFilesAppDialogDelegate(base::WeakPtr<SelectFileDialogExtension> parent,
                               GURL url,
                               std::u16string title)
      : ash::SystemWebDialogDelegate(url, title), parent_(std::move(parent)) {
    RegisterOnDialogClosedCallback(
        base::BindOnce(&SystemFilesAppDialogDelegate::OnDialogClosing,
                       base::Unretained(this)));
    // Disable early accelerator processing to give the webcontent the first
    // chance to handle keyboard events (e.g. Ctrl+F). Unhandled events will be
    // forwarded back to the widget.
    set_allow_accelerators_deprecated(false);
  }
  SystemFilesAppDialogDelegate(const SystemFilesAppDialogDelegate&) = delete;
  SystemFilesAppDialogDelegate& operator=(const SystemFilesAppDialogDelegate&) =
      delete;
  ~SystemFilesAppDialogDelegate() override = default;

  void SetModalType(ui::mojom::ModalType modal_type) {
    set_dialog_modal_type(modal_type);
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
      parent_->OnSystemDialogShown(webui->GetWebContents());
    }
    ash::SystemWebDialogDelegate::OnDialogShown(webui);
  }

  void OnDialogClosing(const std::string& ret_val) {
    if (parent_) {
      parent_->OnSystemDialogWillClose();
    }
  }

 private:
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
}

void SelectFileDialogExtension::OnSystemDialogShown(
    content::WebContents* web_contents) {
  system_files_app_web_contents_ = web_contents;
  // The user data holds a reference to `this` so that the dialog outlives its
  // caller for as long as the Files app dialog is open.
  SelectFileDialogExtensionUserData::SetDialogDataForWebContents(
      web_contents, this, type_, owner_.dialog_caller);
}

void SelectFileDialogExtension::OnSystemDialogWillClose() {
  profile_ = nullptr;
  auto dialog_caller = owner_.dialog_caller;
  if (owner_.window) {
    owner_.window->ClearProperty(kHasOpenSelectFileDialogKey);
  }
  owner_ = {};
  system_files_app_web_contents_ = nullptr;
  // Actually invoke the appropriate callback on our listener.
  ApplyPolicyAndNotifyListener(std::move(dialog_caller));
}

void SelectFileDialogExtension::OnFileSelected(const ui::SelectedFileInfo& file,
                                               int index) {
  selection_type_ = SelectionType::kSingleFile;
  selection_files_.clear();
  selection_files_.push_back(file);
  selection_index_ = index;
}

void SelectFileDialogExtension::OnMultiFilesSelected(
    const std::vector<ui::SelectedFileInfo>& files) {
  selection_type_ = SelectionType::kMultipleFiles;
  selection_files_ = files;
  selection_index_ = 0;
}

void SelectFileDialogExtension::OnFileSelectionCanceled() {
  selection_type_ = SelectionType::kCancel;
  selection_files_.clear();
  selection_index_ = 0;
}

content::RenderFrameHost* SelectFileDialogExtension::GetPrimaryMainFrame() {
  if (system_files_app_web_contents_) {
    return system_files_app_web_contents_->GetPrimaryMainFrame();
  }
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
    // as |default_path| (crbug.com/40302893 #9-#11). In such a case, we use the
    // last selected directory as a workaround. Real fix is tracked at
    // crbug.com/40138234.
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

  // Another dialog is already open for this window. It's unlikely, but
  // possible. In such situation, discard this request.
  if (owner.window && owner.window->GetProperty(kHasOpenSelectFileDialogKey)) {
    LOG(ERROR) << "File dialog already open for the owner window!";
    return;
  }

  const bool is_for_capture_mode =
      owner.window &&
      owner.window->GetId() ==
          ash::kShellWindowId_CaptureModeFolderSelectionDialogOwner;

  can_resize_ = !display::Screen::Get()->InTabletMode() && !is_for_capture_mode;

  const RuntimeContext context = GetRuntimeContext(owner.window);
  profile_ = context.profile;
  CHECK(profile_, base::NotFatalUntil::M162);

  std::vector<std::string> volume_filter;
  if (use_media_store_filter) {
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

  owner_ = owner;
  type_ = type;
  if (owner_.window) {
    owner_.window->SetProperty(kHasOpenSelectFileDialogKey, true);
  }

  // The dialog is modal to its owner window. If the owner is itself system
  // modal (e.g. a LoginWebDialog on the login screen or during kiosk launch),
  // the dialog has to be system modal too: it is placed in the owner's system
  // modal container, which does not accept window modal children.
  ui::mojom::ModalType modal_type = ui::mojom::ModalType::kNone;
  if (owner.window) {
    modal_type = owner.window->GetProperty(aura::client::kModalKey) ==
                         ui::mojom::ModalType::kSystem
                     ? ui::mojom::ModalType::kSystem
                     : ui::mojom::ModalType::kWindow;
  }

  // The delegate deletes itself in WebDialogDelegate::OnDialogClosed().
  auto* dialog_delegate = new SystemFilesAppDialogDelegate(
      weak_factory_.GetWeakPtr(), file_manager_url, dialog_title);
  dialog_delegate->SetModalType(modal_type);
  dialog_delegate->set_can_resize(can_resize_);
  dialog_delegate->ShowSystemDialogForBrowserContext(profile_,
                                                     context.parent_window);
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
  if (!listener_) {
    return;
  }

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
              if (!is_allowed) {
                weak_ptr->selection_type_ = SelectionType::kCancel;
              }
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
              if (allowed_files.empty()) {
                weak_ptr->selection_type_ = SelectionType::kCancel;
              }
              weak_ptr->NotifyListener(std::move(allowed_files));
            },
            weak_factory_.GetWeakPtr()));
    return;
  }
  NotifyListener(std::move(selection_files));
}

void SelectFileDialogExtension::NotifyListener(
    std::vector<ui::SelectedFileInfo> selection_files) {
  if (!listener_) {
    return;
  }
  switch (selection_type_) {
    case SelectionType::kCancel:
      listener_->FileSelectionCanceled();
      break;
    case SelectionType::kSingleFile:
      listener_->FileSelected(selection_files[0], selection_index_);
      break;
    case SelectionType::kMultipleFiles:
      listener_->MultiFilesSelected(selection_files);
      break;
    default:
      NOTREACHED();
  }
}
