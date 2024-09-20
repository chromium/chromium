// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_win.h"

#include <windows.h>

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/process/process_handle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_window_property_manager_win.h"
#include "chrome/browser/ui/views/frame/system_menu_insertion_delegate_win.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/win/app_icon.h"
#include "chrome/browser/win/titlebar_config.h"
#include "chrome/common/chrome_constants.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/theme_provider.h"
#include "ui/base/win/hwnd_metrics.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image_family.h"
#include "ui/views/controls/menu/native_menu_win.h"

class VirtualDesktopHelper
    : public base::RefCountedDeleteOnSequence<VirtualDesktopHelper> {
 public:
  using WorkspaceChangedCallback = base::OnceCallback<void()>;

  explicit VirtualDesktopHelper(const std::string& initial_workspace);
  VirtualDesktopHelper(const VirtualDesktopHelper&) = delete;
  VirtualDesktopHelper& operator=(const VirtualDesktopHelper&) = delete;

  // public methods are all called on the UI thread.
  void Init(HWND hwnd);

  std::string GetWorkspace();

  // |callback| is called when the task to get the desktop id of |hwnd|
  // completes, if the workspace has changed.
  void UpdateWindowDesktopId(HWND hwnd, WorkspaceChangedCallback callback);

  bool GetInitialWorkspaceRemembered() const;

  void SetInitialWorkspaceRemembered(bool remembered);

 private:
  friend class base::RefCountedDeleteOnSequence<VirtualDesktopHelper>;
  friend class base::DeleteHelper<VirtualDesktopHelper>;

  ~VirtualDesktopHelper();

  // Called on the UI thread as a task reply.
  void SetWorkspace(WorkspaceChangedCallback callback,
                    const std::string& workspace);

  void InitImpl(HWND hwnd, const std::string& initial_workspace);

  static std::string GetWindowDesktopIdImpl(
      HWND hwnd,
      Microsoft::WRL::ComPtr<IVirtualDesktopManager> virtual_desktop_manager);

  // All member variables, except where noted, are only accessed on the ui
  // thead.

  // Workspace browser window was opened on. This is used to tell the
  // BrowserWindowState about the initial workspace, which has to happen after
  // |this| is fully set up.
  const std::string initial_workspace_;

  // On Windows10, this is the virtual desktop the browser window was on,
  // last we checked. This is used to tell if the window has moved to a
  // different desktop, and notify listeners. It will only be set if
  // we created |virtual_desktop_helper_|.
  std::optional<std::string> workspace_;

  bool initial_workspace_remembered_ = false;

  // Only set on Windows 10. This is created and accessed on a separate
  // COMSTAT thread. It will be null if creation failed.
  Microsoft::WRL::ComPtr<IVirtualDesktopManager> virtual_desktop_manager_;

  base::WeakPtrFactory<VirtualDesktopHelper> weak_factory_{this};
};

VirtualDesktopHelper::VirtualDesktopHelper(const std::string& initial_workspace)
    : base::RefCountedDeleteOnSequence<VirtualDesktopHelper>(
          base::ThreadPool::CreateCOMSTATaskRunner(
              {base::MayBlock(),
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      initial_workspace_(initial_workspace) {}

void VirtualDesktopHelper::Init(HWND hwnd) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  owning_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VirtualDesktopHelper::InitImpl, this, hwnd,
                                initial_workspace_));
}

VirtualDesktopHelper::~VirtualDesktopHelper() {}

std::string VirtualDesktopHelper::GetWorkspace() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!workspace_.has_value())
    workspace_ = initial_workspace_;

  return workspace_.value_or(std::string());
}

void VirtualDesktopHelper::UpdateWindowDesktopId(
    HWND hwnd,
    WorkspaceChangedCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  owning_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&VirtualDesktopHelper::GetWindowDesktopIdImpl, hwnd,
                     virtual_desktop_manager_),
      base::BindOnce(&VirtualDesktopHelper::SetWorkspace, this,
                     std::move(callback)));
}

bool VirtualDesktopHelper::GetInitialWorkspaceRemembered() const {
  return initial_workspace_remembered_;
}

void VirtualDesktopHelper::SetInitialWorkspaceRemembered(bool remembered) {
  initial_workspace_remembered_ = remembered;
}

void VirtualDesktopHelper::SetWorkspace(WorkspaceChangedCallback callback,
                                        const std::string& workspace) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // If GetWindowDesktopId() fails, |workspace| will be empty, and it's most
  // likely that the current value of |workspace_| is still correct, so don't
  // overwrite it.
  if (workspace.empty())
    return;

  bool workspace_changed = workspace != workspace_.value_or(std::string());
  workspace_ = workspace;
  if (workspace_changed)
    std::move(callback).Run();
}

void VirtualDesktopHelper::InitImpl(HWND hwnd,
                                    const std::string& initial_workspace) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Virtual Desktops on Windows are best-effort and may not always be
  // available.
  if (FAILED(::CoCreateInstance(__uuidof(::VirtualDesktopManager), nullptr,
                                CLSCTX_ALL,
                                IID_PPV_ARGS(&virtual_desktop_manager_))) ||
      initial_workspace.empty()) {
    return;
  }
  GUID guid = GUID_NULL;
  HRESULT hr =
      CLSIDFromString(base::UTF8ToWide(initial_workspace).c_str(), &guid);
  if (SUCCEEDED(hr)) {
    // There are valid reasons MoveWindowToDesktop can fail, e.g.,
    // the desktop was deleted. If it fails, the window will open on the
    // current desktop.
    virtual_desktop_manager_->MoveWindowToDesktop(hwnd, guid);
  }
}

// static
std::string VirtualDesktopHelper::GetWindowDesktopIdImpl(
    HWND hwnd,
    Microsoft::WRL::ComPtr<IVirtualDesktopManager> virtual_desktop_manager) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!virtual_desktop_manager)
    return std::string();

  GUID workspace_guid;
  HRESULT hr =
      virtual_desktop_manager->GetWindowDesktopId(hwnd, &workspace_guid);
  if (FAILED(hr) || workspace_guid == GUID_NULL)
    return std::string();

  LPOLESTR workspace_widestr;
  StringFromCLSID(workspace_guid, &workspace_widestr);
  std::string workspace_id = base::WideToUTF8(workspace_widestr);
  CoTaskMemFree(workspace_widestr);
  return workspace_id;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostWin, public:

BrowserDesktopWindowTreeHostWin::BrowserDesktopWindowTreeHostWin(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame)
    : DesktopWindowTreeHostWin(native_widget_delegate,
                               desktop_native_widget_aura),
      browser_view_(browser_view),
      browser_frame_(browser_frame),
      virtual_desktop_helper_(nullptr) {
  profile_observation_.Observe(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage());

  // TODO(crbug.com/40118412) Make turning off this policy turn off
  // native window occlusion on this browser win.
  if (!g_browser_process->local_state()->GetBoolean(
          policy::policy_prefs::kNativeWindowOcclusionEnabled)) {
    SetNativeWindowOcclusionEnabled(false);
  }
}

BrowserDesktopWindowTreeHostWin::~BrowserDesktopWindowTreeHostWin() {}

views::NativeMenuWin* BrowserDesktopWindowTreeHostWin::GetSystemMenu() {
  if (!system_menu_.get()) {
    SystemMenuInsertionDelegateWin insertion_delegate;
    system_menu_ = std::make_unique<views::NativeMenuWin>(
        browser_frame_->GetSystemMenuModel(), GetHWND());
    system_menu_->Rebuild(&insertion_delegate);
  }
  return system_menu_.get();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostWin, BrowserDesktopWindowTreeHost implementation:

views::DesktopWindowTreeHost*
    BrowserDesktopWindowTreeHostWin::AsDesktopWindowTreeHost() {
  return this;
}

int BrowserDesktopWindowTreeHostWin::GetMinimizeButtonOffset() const {
  return minimize_button_metrics_.GetMinimizeButtonOffsetX();
}

bool BrowserDesktopWindowTreeHostWin::UsesNativeSystemMenu() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostWin, views::DesktopWindowTreeHostWin overrides:

void BrowserDesktopWindowTreeHostWin::Init(
    const views::Widget::InitParams& params) {
  DesktopWindowTreeHostWin::Init(params);
  virtual_desktop_helper_ = new VirtualDesktopHelper(params.workspace);
  virtual_desktop_helper_->Init(GetHWND());
}

void BrowserDesktopWindowTreeHostWin::Show(
    ui::mojom::WindowShowState show_state,
    const gfx::Rect& restore_bounds) {
  // This will make BrowserWindowState remember the initial workspace.
  // It has to be called after DesktopNativeWidgetAura is observing the host
  // and the session service is tracking the window.
  if (virtual_desktop_helper_ &&
      !virtual_desktop_helper_->GetInitialWorkspaceRemembered()) {
    // If |virtual_desktop_helper_| has an empty workspace, kick off an update,
    // which will eventually call OnHostWorkspaceChanged.
    if (virtual_desktop_helper_->GetWorkspace().empty())
      UpdateWorkspace();
    else
      OnHostWorkspaceChanged();
  }
  DesktopWindowTreeHostWin::Show(show_state, restore_bounds);
}

void BrowserDesktopWindowTreeHostWin::HandleWindowMinimizedOrRestored(
    bool restored) {
  DesktopWindowTreeHostWin::HandleWindowMinimizedOrRestored(restored);

  // This is necessary since OnWidgetVisibilityChanged() doesn't get called on
  // Windows when the window is minimized or restored.
  if (base::FeatureList::IsEnabled(
          features::kStopLoadingAnimationForHiddenWindow)) {
    browser_view_->UpdateLoadingAnimations(restored);
  }
}

std::string BrowserDesktopWindowTreeHostWin::GetWorkspace() const {
  return virtual_desktop_helper_ ? virtual_desktop_helper_->GetWorkspace()
                                 : std::string();
}

int BrowserDesktopWindowTreeHostWin::GetInitialShowState() const {
  STARTUPINFO si = {0};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  GetStartupInfo(&si);
  return si.wShowWindow;
}

bool BrowserDesktopWindowTreeHostWin::GetClientAreaInsets(
    gfx::Insets* insets,
    HMONITOR monitor) const {
  // Always use default insets for opaque frame.
  if (!ShouldUseNativeFrame())
    return false;

  // Use default insets for popups and apps, unless we are custom drawing the
  // titlebar.
  if (!ShouldBrowserCustomDrawTitlebar(browser_view_) &&
      !browser_view_->GetIsNormalType()) {
    return false;
  }

  if (GetWidget()->IsFullscreen()) {
    // In fullscreen mode there is no frame.
    *insets = gfx::Insets();
  } else {
    const int frame_thickness = ui::GetFrameThickness(monitor);
    // Reduce the non-client border size; UpdateDWMFrame() will instead extend
    // the border into the window client area. For maximized windows, Windows
    // outdents the window rect from the screen's client rect by
    // |frame_thickness| on each edge, meaning |insets| must contain
    // |frame_thickness| on all sides (including the top) to avoid the client
    // area extending onto adjacent monitors. For non-maximized windows,
    // however, the top inset must be zero, since if there is any nonclient
    // area, Windows will draw a full native titlebar outside the client area.
    // (This doesn't occur in the maximized case.)
    int top_thickness = 0;
    if (ShouldBrowserCustomDrawTitlebar(browser_view_) &&
        GetWidget()->IsMaximized()) {
      top_thickness = frame_thickness;
    }
    *insets = gfx::Insets::TLBR(top_thickness, frame_thickness, frame_thickness,
                                frame_thickness);
  }
  return true;
}

bool BrowserDesktopWindowTreeHostWin::GetDwmFrameInsetsInPixels(
    gfx::Insets* insets) const {
  // For "normal" windows on Aero, we always need to reset the glass area
  // correctly, even if we're not currently showing the native frame (e.g.
  // because a theme is showing), so we explicitly check for that case rather
  // than checking ShouldUseNativeFrame() here.  Using that here would mean we
  // wouldn't reset the glass area to zero when moving from the native frame to
  // an opaque frame, leading to graphical glitches behind the opaque frame.
  // Instead, we use that function below to tell us whether the frame is
  // currently native or opaque.
  if (!GetWidget()->client_view() || !browser_view_->GetIsNormalType() ||
      !DesktopWindowTreeHostWin::ShouldUseNativeFrame())
    return false;

  // Don't extend the glass in at all if it won't be visible.
  if (!ShouldUseNativeFrame() || GetWidget()->IsFullscreen() ||
      ShouldBrowserCustomDrawTitlebar(browser_view_)) {
    *insets = gfx::Insets();
  } else {
    // The glass should extend to the bottom of the tabstrip.
    gfx::Rect tabstrip_region_bounds(browser_frame_->GetBoundsForTabStripRegion(
        browser_view_->tab_strip_region_view()->GetMinimumSize()));
    tabstrip_region_bounds = display::win::ScreenWin::DIPToClientRect(
        GetHWND(), tabstrip_region_bounds);

    *insets = gfx::Insets::TLBR(tabstrip_region_bounds.bottom(), 0, 0, 0);
  }
  return true;
}

void BrowserDesktopWindowTreeHostWin::HandleCreate() {
  DesktopWindowTreeHostWin::HandleCreate();
  browser_window_property_manager_ =
      BrowserWindowPropertyManager::CreateBrowserWindowPropertyManager(
          browser_view_, GetHWND());

  // Use the profile icon as the browser window icon, if there is more
  // than one profile. This makes alt-tab preview tabs show the profile-specific
  // icon in the multi-profile case.
  if (g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetNumberOfProfiles() > 1) {
    SetWindowIcon(/*badged=*/true);
  }
}

void BrowserDesktopWindowTreeHostWin::HandleDestroying() {
  browser_window_property_manager_.reset();
  DesktopWindowTreeHostWin::HandleDestroying();
}

void BrowserDesktopWindowTreeHostWin::HandleWindowScaleFactorChanged(
    float window_scale_factor) {
  DesktopWindowTreeHostWin::HandleWindowScaleFactorChanged(window_scale_factor);
  minimize_button_metrics_.OnDpiChanged();
}

bool BrowserDesktopWindowTreeHostWin::PreHandleMSG(UINT message,
                                                   WPARAM w_param,
                                                   LPARAM l_param,
                                                   LRESULT* result) {
  switch (message) {
    case WM_ACTIVATE:
      if (LOWORD(w_param) != WA_INACTIVE)
        minimize_button_metrics_.OnHWNDActivated();
      return false;
    case WM_ENDSESSION:
      chrome::SessionEnding();
      return true;
    case WM_INITMENUPOPUP:
      GetSystemMenu()->UpdateStates();
      return true;
  }
  return DesktopWindowTreeHostWin::PreHandleMSG(
      message, w_param, l_param, result);
}

void BrowserDesktopWindowTreeHostWin::PostHandleMSG(UINT message,
                                                    WPARAM w_param,
                                                    LPARAM l_param) {
  switch (message) {
    case WM_SETFOCUS: {
      UpdateWorkspace();
      break;
    }
    case WM_CREATE:
      minimize_button_metrics_.Init(GetHWND());
      break;
    case WM_WINDOWPOSCHANGED: {
      // Windows lies to us about the position of the minimize button before a
      // window is visible. We use this position to place the incognito avatar
      // in RTL mode, so when the window is shown, we need to re-layout and
      // schedule a paint for the non-client frame view so that the icon top has
      // the correct position when the window becomes visible. This fixes bugs
      // where the icon appears to overlay the minimize button. Note that we
      // will call Layout every time SetWindowPos is called with SWP_SHOWWINDOW,
      // however callers typically are careful about not specifying this flag
      // unless necessary to avoid flicker. This may be invoked during creation
      // on XP and before the non_client_view has been created.
      WINDOWPOS* window_pos = reinterpret_cast<WINDOWPOS*>(l_param);
      views::NonClientView* non_client_view = GetWidget()->non_client_view();
      if (window_pos->flags & SWP_SHOWWINDOW && non_client_view) {
        non_client_view->DeprecatedLayoutImmediately();
        non_client_view->SchedulePaint();
      }
      break;
    }
    case WM_DWMCOLORIZATIONCOLORCHANGED: {
      // The activation border may have changed color.
      views::NonClientView* non_client_view = GetWidget()->non_client_view();
      if (non_client_view)
        non_client_view->SchedulePaint();
      break;
    }
  }
}

views::FrameMode BrowserDesktopWindowTreeHostWin::GetFrameMode() const {
  const views::FrameMode system_frame_mode =
      ShouldBrowserCustomDrawTitlebar(browser_view_)
          ? views::FrameMode::SYSTEM_DRAWN_NO_CONTROLS
          : views::FrameMode::SYSTEM_DRAWN;

  // We don't theme popup or app windows, so regardless of whether or not a
  // theme is active for normal browser windows, we don't want to use the custom
  // frame for popups/apps.
  if (!browser_view_->GetIsNormalType() &&
      DesktopWindowTreeHostWin::GetFrameMode() ==
          views::FrameMode::SYSTEM_DRAWN) {
    return system_frame_mode;
  }

  // Otherwise, we use the native frame when we're told we should by the theme
  // provider (e.g. no custom theme is active).
  return GetWidget()->GetThemeProvider()->ShouldUseNativeFrame()
             ? system_frame_mode
             : views::FrameMode::CUSTOM_DRAWN;
}

bool BrowserDesktopWindowTreeHostWin::ShouldUseNativeFrame() const {
  if (!views::DesktopWindowTreeHostWin::ShouldUseNativeFrame())
    return false;
  // This function can get called when the Browser window is closed i.e. in the
  // context of the BrowserView destructor.
  if (!browser_view_->browser())
    return false;

  // We don't theme popup or app windows, so regardless of whether or not a
  // theme is active for normal browser windows, we don't want to use the custom
  // frame for popups/apps.
  if (!browser_view_->GetIsNormalType())
    return true;
  // Otherwise, we use the native frame when we're told we should by the theme
  // provider (e.g. no custom theme is active).
  return GetWidget()->GetThemeProvider()->ShouldUseNativeFrame();
}

bool BrowserDesktopWindowTreeHostWin::ShouldWindowContentsBeTransparent()
    const {
  return !ShouldBrowserCustomDrawTitlebar(browser_view_) &&
         views::DesktopWindowTreeHostWin::ShouldWindowContentsBeTransparent();
}

////////////////////////////////////////////////////////////////////////////////
// ProfileAttributesStorage::Observer overrides:

void BrowserDesktopWindowTreeHostWin::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  // If we're currently badging the window icon (>1 available profile),
  // and this window's profile's avatar changed, update the window icon.
  if (browser_view_->browser()->profile()->GetPath() == profile_path &&
      g_browser_process->profile_manager()
              ->GetProfileAttributesStorage()
              .GetNumberOfProfiles() > 1) {
    // If we went from 1 to 2 profiles, window icons should be badged.
    SetWindowIcon(/*badged=*/true);
  }
}

void BrowserDesktopWindowTreeHostWin::OnProfileAdded(
    const base::FilePath& profile_path) {
  if (g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetNumberOfProfiles() == 2) {
    SetWindowIcon(/*badged=*/true);
  }
}

void BrowserDesktopWindowTreeHostWin::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const std::u16string& profile_name) {
  if (g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetNumberOfProfiles() == 1) {
    // If we went from 2 profiles to 1, window icons should not be badged.
    SetWindowIcon(/*badged=*/false);
  }
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostWin, private:

void BrowserDesktopWindowTreeHostWin::UpdateWorkspace() {
  if (!virtual_desktop_helper_)
    return;
  virtual_desktop_helper_->UpdateWindowDesktopId(
      GetHWND(),
      base::BindOnce(&BrowserDesktopWindowTreeHostWin::OnHostWorkspaceChanged,
                     weak_factory_.GetWeakPtr()));
}

SkBitmap GetBadgedIconBitmapForProfile(Profile* profile) {
  std::unique_ptr<gfx::ImageFamily> family = GetAppIconImageFamily();
  if (!family)
    return SkBitmap();

  SkBitmap app_icon_bitmap = family
                                 ->CreateExact(profiles::kShortcutIconSizeWin,
                                               profiles::kShortcutIconSizeWin)
                                 .AsBitmap();
  if (app_icon_bitmap.isNull())
    return SkBitmap();

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (!entry)
    return SkBitmap();

  SkBitmap avatar_bitmap_2x = profiles::GetWin2xAvatarImage(entry);
  return profiles::GetBadgedWinIconBitmapForAvatar(app_icon_bitmap,
                                                   avatar_bitmap_2x);
}

void BrowserDesktopWindowTreeHostWin::SetWindowIcon(bool badged) {
  // Hold onto the previous icon so that the currently displayed
  // icon is valid until replaced with the new icon.
  base::win::ScopedHICON previous_icon = std::move(icon_handle_);
  if (badged) {
    icon_handle_ = IconUtil::CreateHICONFromSkBitmap(
        GetBadgedIconBitmapForProfile(browser_view_->browser()->profile()));
  } else {
    icon_handle_.reset(GetAppIcon());
  }
  SendMessage(GetHWND(), WM_SETICON, ICON_SMALL,
              reinterpret_cast<LPARAM>(icon_handle_.get()));
  SendMessage(GetHWND(), WM_SETICON, ICON_BIG,
              reinterpret_cast<LPARAM>(icon_handle_.get()));
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHost, public:

// static
BrowserDesktopWindowTreeHost*
    BrowserDesktopWindowTreeHost::CreateBrowserDesktopWindowTreeHost(
        views::internal::NativeWidgetDelegate* native_widget_delegate,
        views::DesktopNativeWidgetAura* desktop_native_widget_aura,
        BrowserView* browser_view,
        BrowserFrame* browser_frame) {
  return new BrowserDesktopWindowTreeHostWin(native_widget_delegate,
                                             desktop_native_widget_aura,
                                             browser_view, browser_frame);
}
