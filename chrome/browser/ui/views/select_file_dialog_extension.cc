// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_file_dialog_extension.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/platform_apps/app_window_registry_util.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/login_web_dialog.h"
#include "chrome/browser/ash/login/ui/webui_login_view.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/extensions/file_manager/select_file_dialog_extension_user_data.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/select_file_dialog_util.h"
#include "chrome/browser/chromeos/file_manager/url_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/extensions/extension_dialog.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_system.h"
#include "ui/base/base_window.h"
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

// Specific color for File Picker (Files app).
// TODO(crbug/1072904): Get these colors from ui::NativeTheme.
constexpr SkColor kFilePickerActiveTitleColor = gfx::kGoogleGrey200;
constexpr SkColor kFilePickerInactiveTitleColor = gfx::kGoogleGrey200;

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
  chromeos::LoginDisplayHost* host = chromeos::LoginDisplayHost::default_host();
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
  if (chrome::IsRunningInForcedAppMode() && !(*web_contents))
    *web_contents = chromeos::LoginWebDialog::GetCurrentWebContents();

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
        "web.%d", web_contents->GetMainFrame()->GetFrameTreeNodeId());
  }
  LOG(ERROR) << "Unable to generate a RoutingID";
  return "";
}

}  // namespace

/////////////////////////////////////////////////////////////////////////////

SelectFileDialogExtension::Owner::Owner() = default;
SelectFileDialogExtension::Owner::~Owner() = default;

// static
SelectFileDialogExtension* SelectFileDialogExtension::Create(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return new SelectFileDialogExtension(listener, std::move(policy));
}

SelectFileDialogExtension::SelectFileDialogExtension(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : SelectFileDialog(listener, std::move(policy)) {}

SelectFileDialogExtension::~SelectFileDialogExtension() {
  if (extension_dialog_.get())
    extension_dialog_->ObserverDestroyed();
}

bool SelectFileDialogExtension::IsRunning(
    gfx::NativeWindow owner_window) const {
  return owner_window_ == owner_window;
}

void SelectFileDialogExtension::ListenerDestroyed() {
  listener_ = nullptr;
  params_ = nullptr;
  PendingDialog::GetInstance()->Remove(routing_id_);
}

void SelectFileDialogExtension::ExtensionDialogClosing(
    ExtensionDialog* /*dialog*/) {
  profile_ = nullptr;
  owner_window_ = nullptr;
  // Release our reference to the underlying dialog to allow it to close.
  extension_dialog_ = nullptr;
  PendingDialog::GetInstance()->Remove(routing_id_);
  // Actually invoke the appropriate callback on our listener.
  NotifyListener();
}

void SelectFileDialogExtension::ExtensionTerminated(
    ExtensionDialog* dialog) {
  // The extension would have been unloaded because of the termination,
  // reload it.
  std::string extension_id = dialog->host()->extension()->id();
  // Reload the extension after a bit; the extension may not have been unloaded
  // yet. We don't want to try to reload the extension only to have the Unload
  // code execute after us and re-unload the extension.
  //
  // TODO(rkc): This is ugly. The ideal solution is that we shouldn't need to
  // reload the extension at all - when we try to open the extension the next
  // time, the extension subsystem would automatically reload it for us. At
  // this time though this is broken because of some faulty wiring in
  // extensions::ProcessManager::CreateViewHost. Once that is fixed, remove
  // this.
  if (profile_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &extensions::ExtensionService::ReloadExtension,
            base::Unretained(extensions::ExtensionSystem::Get(profile_)
                                 ->extension_service()),
            extension_id));
  }

  dialog->GetWidget()->Close();
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

content::RenderFrameHost* SelectFileDialogExtension::GetMainFrame() {
  if (extension_dialog_)
    return extension_dialog_->host()->main_frame_host();
  return nullptr;
}

void SelectFileDialogExtension::SelectFileWithFileManagerParams(
    Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    void* params,
    const Owner& owner,
    const std::string& search_query,
    bool show_android_picker_apps) {
  if (owner_window_) {
    LOG(ERROR) << "File dialog already in use!";
    return;
  }

  // The base window to associate the dialog with.
  ui::BaseWindow* base_window = nullptr;

  // The web contents to associate the dialog with.
  content::WebContents* web_contents = nullptr;

  // Obtain BaseWindow and WebContents if the owner window is browser.
  if (!owner.android_task_id.has_value() && !owner.lacros_window_id.has_value())
    FindRuntimeContext(owner.window, &base_window, &web_contents);

  if (web_contents)
    profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // Handle the cases where |web_contents| is not available or |web_contents| is
  // associated with Default profile.
  if (!web_contents || chromeos::ProfileHelper::IsSigninProfile(profile_))
    profile_ = ProfileManager::GetActiveUserProfile();

  DCHECK(profile_);

  // Check if we have another dialog opened for the contents. It's unlikely, but
  // possible. In such situation, discard this request.
  RoutingID routing_id = GetRoutingID(web_contents, owner);
  if (PendingExists(routing_id))
    return;

  base::FilePath download_default_path(
      DownloadPrefs::FromBrowserContext(profile_)->DownloadPath());

  base::FilePath selection_path = default_path.IsAbsolute() ?
      default_path : download_default_path.Append(default_path.BaseName());

  base::FilePath fallback_path = profile_->last_selected_directory().empty() ?
      download_default_path : profile_->last_selected_directory();

  // Convert the above absolute paths to file system URLs.
  GURL selection_url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
      profile_,
      selection_path,
      file_manager::kFileManagerAppId,
      &selection_url)) {
    // Due to the current design, an invalid temporal cache file path may passed
    // as |default_path| (crbug.com/178013 #9-#11). In such a case, we use the
    // last selected directory as a workaround. Real fix is tracked at
    // crbug.com/110119.
    if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
        profile_,
        fallback_path.Append(default_path.BaseName()),
        file_manager::kFileManagerAppId,
        &selection_url)) {
      DVLOG(1) << "Unable to resolve the selection URL.";
    }
  }

  GURL current_directory_url;
  base::FilePath current_directory_path = selection_path.DirName();
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
      profile_,
      current_directory_path,
      file_manager::kFileManagerAppId,
      &current_directory_url)) {
    // Fallback if necessary, see the comment above.
    if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
            profile_,
            fallback_path,
            file_manager::kFileManagerAppId,
            &current_directory_url)) {
      DVLOG(1) << "Unable to resolve the current directory URL for: "
               << fallback_path.value();
    }
  }

  has_multiple_file_type_choices_ =
      !file_types || (file_types->extensions.size() > 1);

  GURL file_manager_url =
      file_manager::util::GetFileManagerMainPageUrlWithParams(
          type, title, current_directory_url, selection_url,
          default_path.BaseName().value(), file_types, file_type_index,
          search_query, show_android_picker_apps);

  ExtensionDialog::InitParams dialog_params(
      {kFileManagerWidth, kFileManagerHeight});
  dialog_params.is_modal = (owner.window != nullptr);
  dialog_params.min_size = {kFileManagerMinimumWidth,
                            kFileManagerMinimumHeight};
  dialog_params.title =
      !title.empty() ? title
                     : file_manager::util::GetSelectFileDialogTitle(type);
  if (base::FeatureList::IsEnabled(chromeos::features::kFilesNG)) {
    dialog_params.title_color = kFilePickerActiveTitleColor;
    dialog_params.title_inactive_color = kFilePickerInactiveTitleColor;
  }

  ExtensionDialog* dialog = ExtensionDialog::Show(
      file_manager_url,
      base_window ? base_window->GetNativeWindow() : owner.window, profile_,
      web_contents, this /* ExtensionDialog::Observer */, dialog_params);
  if (!dialog) {
    LOG(ERROR) << "Unable to create extension dialog";
    return;
  }

  SelectFileDialogExtensionUserData::SetRoutingIdForWebContents(
      dialog->host()->host_contents(), routing_id);

  // Connect our listener to FileDialogFunction's per-tab callbacks.
  AddPending(routing_id);

  extension_dialog_ = dialog;
  params_ = params;
  routing_id_ = routing_id;
  owner_window_ = owner.window;
}

void SelectFileDialogExtension::SelectFileImpl(
    Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owner_window,
    void* params) {
  // |default_extension| is ignored.
  Owner owner;
  owner.window = owner_window;
  SelectFileWithFileManagerParams(type, title, default_path, file_types,
                                  file_type_index, params, owner,
                                  /*search_query=*/"",
                                  /*show_android_picker_apps=*/false);
}

bool SelectFileDialogExtension::HasMultipleFileTypeChoicesImpl() {
  return has_multiple_file_type_choices_;
}

bool SelectFileDialogExtension::IsResizeable() const {
  DCHECK(extension_dialog_.get());
  return extension_dialog_->CanResize();
}

void SelectFileDialogExtension::NotifyListener() {
  if (!listener_)
    return;
  switch (selection_type_) {
    case CANCEL:
      listener_->FileSelectionCanceled(params_);
      break;
    case SINGLE_FILE:
      listener_->FileSelectedWithExtraInfo(selection_files_[0],
                                           selection_index_, params_);
      break;
    case MULTIPLE_FILES:
      listener_->MultiFilesSelectedWithExtraInfo(selection_files_, params_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void SelectFileDialogExtension::AddPending(RoutingID routing_id) {
  PendingDialog::GetInstance()->Add(routing_id, this);
}

// static
bool SelectFileDialogExtension::PendingExists(RoutingID routing_id) {
  return PendingDialog::GetInstance()->Find(routing_id).get() != nullptr;
}
