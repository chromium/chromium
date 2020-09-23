// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "chrome/browser/shell_integration_win.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace {

ProfilePickerView* g_profile_picker_view = nullptr;
constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 758;
constexpr float kMaxRatioOfWorkArea = 0.9;

GURL CreateURLForEntryPoint(ProfilePicker::EntryPoint entry_point) {
  GURL base_url = GURL(chrome::kChromeUIProfilePickerUrl);
  switch (entry_point) {
    case ProfilePicker::EntryPoint::kOnStartup: {
      GURL::Replacements replacements;
      replacements.SetQueryStr(chrome::kChromeUIProfilePickerStartupQuery);
      return base_url.ReplaceComponents(replacements);
    }
    case ProfilePicker::EntryPoint::kProfileMenuManageProfiles:
    case ProfilePicker::EntryPoint::kOpenNewWindowAfterProfileDeletion:
      return base_url;
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile:
      return base_url.Resolve("new-profile");
  }
}

}  // namespace

// static
void ProfilePicker::Show(EntryPoint entry_point) {
  if (!g_profile_picker_view)
    g_profile_picker_view = new ProfilePickerView();

  g_profile_picker_view->Display(entry_point);
}

// static
void ProfilePicker::SwitchToSignIn(SkColor profile_color,
                                   base::OnceClosure switch_failure_callback) {
  if (g_profile_picker_view) {
    g_profile_picker_view->SwitchToSignIn(profile_color,
                                          std::move(switch_failure_callback));
  }
}

// static
void ProfilePicker::Hide() {
  if (g_profile_picker_view)
    g_profile_picker_view->Clear();
}

// static
bool ProfilePicker::IsOpen() {
  return g_profile_picker_view;
}

ProfilePickerView::ProfilePickerView()
    : keep_alive_(KeepAliveOrigin::USER_MANAGER_VIEW,
                  KeepAliveRestartOption::DISABLED) {
  SetHasWindowSizeControls(true);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetTitle(IDS_PRODUCT_NAME);
  set_use_custom_frame(false);
  // TODO(crbug.com/1063856): Add |RecordDialogCreation|.
}

ProfilePickerView::~ProfilePickerView() = default;

void ProfilePickerView::Display(ProfilePicker::EntryPoint entry_point) {
  // Record creation metrics.
  base::UmaHistogramEnumeration("ProfilePicker.Shown", entry_point);
  if (entry_point == ProfilePicker::EntryPoint::kOnStartup) {
    DCHECK(creation_time_on_startup_.is_null());
    // Display() is called right after the creation of this object.
    creation_time_on_startup_ = base::TimeTicks::Now();
    base::UmaHistogramTimes("ProfilePicker.StartupTime.BeforeCreation",
                            creation_time_on_startup_ -
                                startup_metric_utils::MainEntryPointTicks());
  }

  if (initialized_ == kNotInitialized) {
    initialized_ = kInProgress;
    g_browser_process->profile_manager()->CreateProfileAsync(
        ProfileManager::GetSystemProfilePath(),
        base::BindRepeating(&ProfilePickerView::OnSystemProfileCreated,
                            weak_ptr_factory_.GetWeakPtr(), entry_point),
        /*name=*/base::string16(), /*icon_url=*/std::string());
    return;
  }

  if (initialized_ == kInProgress)
    return;

  GetWidget()->Activate();
}

void ProfilePickerView::Clear() {
  if (initialized_ == kDone) {
    GetWidget()->Close();
    return;
  }

  WindowClosing();
  DeleteDelegate();
}

void ProfilePickerView::OnSystemProfileCreated(
    ProfilePicker::EntryPoint entry_point,
    Profile* system_profile,
    Profile::CreateStatus status) {
  DCHECK_NE(status, Profile::CREATE_STATUS_LOCAL_FAIL);
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  Init(entry_point, system_profile);
}

void ProfilePickerView::Init(ProfilePicker::EntryPoint entry_point,
                             Profile* system_profile) {
  DCHECK_EQ(initialized_, kInProgress);
  web_view_ = new views::WebView(system_profile);
  web_view_->GetWebContents()->SetDelegate(this);
  // To record metrics using javascript, extensions are needed.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_view_->GetWebContents());
  AddChildView(web_view_);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  CreateDialogWidget(this, nullptr, nullptr);

#if defined(OS_WIN)
  // Set the app id for the user manager to the app id of its parent.
  ui::win::SetAppIdForWindow(
      shell_integration::win::GetAppUserModelIdForBrowser(
          system_profile->GetPath()),
      views::HWNDForWidget(GetWidget()));
#endif

  web_view_->LoadInitialURL(CreateURLForEntryPoint(entry_point));
  GetWidget()->Show();
  web_view_->RequestFocus();
  initialized_ = InitState::kDone;

  if (entry_point == ProfilePicker::EntryPoint::kOnStartup) {
    DCHECK(!creation_time_on_startup_.is_null());
    base::UmaHistogramTimes("ProfilePicker.StartupTime.WebViewCreated",
                            base::TimeTicks::Now() - creation_time_on_startup_);
  }
}

void ProfilePickerView::SwitchToSignIn(
    SkColor profile_color,
    base::OnceClosure switch_failure_callback) {
  DCHECK(!switch_failure_callback_);
  switch_failure_callback_ = std::move(switch_failure_callback);
  size_t icon_index = profiles::GetPlaceholderAvatarIndex();
  // Silently create the new profile for browsing on GAIA (so that the sign-in
  // cookies are stored in the right profile).
  ProfileManager::CreateMultiProfileAsync(
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .ChooseNameForNewProfile(icon_index),
      profiles::GetDefaultAvatarIconUrl(icon_index),
      base::BindRepeating(&ProfilePickerView::OnProfileForSigninCreated,
                          weak_ptr_factory_.GetWeakPtr(), profile_color));
}

void ProfilePickerView::OnProfileForSigninCreated(
    SkColor profile_color,
    Profile* profile,
    Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_LOCAL_FAIL) {
    if (switch_failure_callback_)
      std::move(switch_failure_callback_).Run();
    return;
  } else if (status != Profile::CREATE_STATUS_INITIALIZED) {
    return;
  }

  // No need to report failure any more, delete the callback.
  DCHECK(switch_failure_callback_);
  switch_failure_callback_ = base::OnceClosure();

  DCHECK(profile);

  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
    NOTREACHED();
    return;
  }

  // Mark this profile ephemeral so that it is deleted upon next startup if the
  // browser crashes before finishing the flow.
  entry->SetIsEphemeral(true);

  // Apply a new color to the profile.
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile);
  theme_service->BuildAutogeneratedThemeFromColor(profile_color);

  // Rebuild the view.
  // TODO(crbug.com/1126913): Add the simple toolbar with the back button.
  RemoveAllChildViews(true);
  auto web_view = std::make_unique<views::WebView>(profile);
  web_view->GetWebContents()->SetDelegate(this);
  web_view_ = AddChildView(std::move(web_view));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  web_view_->LoadInitialURL(GaiaUrls::GetInstance()->signin_chrome_sync_dice());
  web_view_->RequestFocus();
}

gfx::Size ProfilePickerView::CalculatePreferredSize() const {
  gfx::Size preferred_size = gfx::Size(kWindowWidth, kWindowHeight);
  gfx::Size work_area_size = GetWidget()->GetWorkAreaBoundsInScreen().size();
  // Keep the window smaller then |work_area_size| so that it feels more like a
  // dialog then like the actual Chrome window.
  gfx::Size max_dialog_size = ScaleToFlooredSize(
      work_area_size, kMaxRatioOfWorkArea, kMaxRatioOfWorkArea);
  preferred_size.SetToMin(max_dialog_size);
  return preferred_size;
}

void ProfilePickerView::WindowClosing() {
  // Now that the window is closed, we can allow a new one to be opened.
  // (WindowClosing comes in asynchronously from the call to Close() and we
  // may have already opened a new instance).
  if (g_profile_picker_view == this)
    g_profile_picker_view = nullptr;
}

gfx::Size ProfilePickerView::GetMinimumSize() const {
  // On small screens, the preferred size may be smaller than the picker
  // minimum size. In that case there will be scrollbars on the picker.
  gfx::Size minimum_size = GetPreferredSize();
  minimum_size.SetToMin(ProfilePickerUI::GetMinimumSize());
  return minimum_size;
}

bool ProfilePickerView::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}
