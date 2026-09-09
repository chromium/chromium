// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_MANAGER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_MANAGER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_observer.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/context_menu_params.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom-forward.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class Profile;
class ScopedKeepAlive;
class SkBitmap;

namespace views {
class MenuRunner;
class UnhandledKeyboardEventHandler;
}  // namespace views

namespace omnibox_everywhere {

#if defined(USE_AURA)
class OmniboxEverywhereEventHandlerAura;
#endif
class OmniboxEverywhereRegionSelectOverlay;
class OmniboxEverywhereWidgetDelegate;

// Manages the desktop Omnibox Everywhere native window (views::Widget)
// lifecycle and handles switching between different profiles.
// TODO(b/543460015): Factor out ui::SimpleMenuModel::Delegate and
// views::ContextMenuController implementation into a dedicated
// OmniboxEverywhereContextMenuController class.
class OmniboxEverywhereUIManager : public views::WidgetObserver,
                                   public WebUIContentsWrapper::Host,
                                   public BrowserCollectionObserver,
                                   public ui::SimpleMenuModel::Delegate,
                                   public views::ContextMenuController,
                                   public PermissionPromptObserver::Observer {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOmniboxEverywhereElementId);

  // Fixed popup window width:
  //   680px (Loomnibox searchbox content width)
  // +  48px (24px left + 24px right body padding in omnibox_everywhere.html to
  //          accommodate the drop shadow without clipping).
  // = 728px total window width.
  static constexpr int kPopupFixedWidth = 728;
  static constexpr int kDefaultRestingHeight = 152;
  static constexpr base::TimeDelta kActivationGracePeriod =
      base::Milliseconds(500);

  enum ContextMenuCommandId {
    kUndo = 1,
    kCut = 2,
    kCopy = 3,
    kPaste = 4,
    kPasteAndSearch = 5,
    kDelete = 6,
    kSelectAll = 7,
    kManageSearchEngines = 8,
    kAlwaysShowAiMode = 9,
    kShowShortcuts = 10,
    kCustomizeKeyboardShortcut = 11,
    kSettings = 12,
  };

  using ContentsWrapperFactory =
      base::RepeatingCallback<std::unique_ptr<WebUIContentsWrapper>(Profile*)>;

  explicit OmniboxEverywhereUIManager(
      ContentsWrapperFactory contents_wrapper_factory = {});
  OmniboxEverywhereUIManager(const OmniboxEverywhereUIManager&) = delete;
  OmniboxEverywhereUIManager& operator=(const OmniboxEverywhereUIManager&) =
      delete;
  ~OmniboxEverywhereUIManager() override;

  void ShowForProfile(Profile* profile,
                      gfx::NativeWindow context = gfx::NativeWindow());

  // Closes the Omnibox Everywhere widget.
  void Close();

  // Demotes the widget to normal Z-order and deactivates it without hiding.
  void Demote();

  // Synchronously closes the widget and destroys the WebContents during profile
  // shutdown.
  void Shutdown();

  // Returns true if the widget is visible.
  bool IsVisible() const;

  // Returns true if the widget is active/focused.
  bool IsActive() const;

  // Returns true if a file chooser, drive picker, or screenshare picker modal
  // dialog is open.
  bool HasOpenModalDialog() const;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetUserDragStarted(views::Widget* widget) override;
  void OnWidgetUserDragEnded(views::Widget* widget) override;

  // WebUIContentsWrapper::Host:
  void CloseUI() override;
  void ShowUI() override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents) override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdChecked(int command_id) const override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  // PermissionPromptObserver::Observer:
  void OnPermissionPromptChanged(bool is_showing,
                                 const gfx::Size& prompt_size) override;

  void OnFileChooserOpened();
  void OnFileChooserClosed();

  void OnDrivePickerOpened();
  void OnDrivePickerClosed();

  using RegionCaptureSource = OmniboxEverywhereService::RegionCaptureSource;

  void OnScreensharePickerOpened();
  void OnScreensharePickerClosed();

  void ShowScreenshotDisclosureDialog(
      base::OnceClosure on_accepted,
      base::OnceClosure on_cancelled = base::DoNothing());
  using RegionSelectedCallback =
      base::OnceCallback<void(const SkBitmap& result_bitmap)>;
  void ShowRegionSelectOverlay(const SkBitmap& screenshot,
                               const RegionCaptureSource& source,
                               RegionSelectedCallback callback);
  void OnRegionSelectOverlayClosed(RegionSelectedCallback callback,
                                   const SkBitmap& result_bitmap);

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override {}
  void OnBrowserClosed(BrowserWindowInterface* browser) override;
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserDeactivated(BrowserWindowInterface* browser) override {}

  Profile* profile() { return profile_; }
  const Profile* profile() const { return profile_; }
  views::Widget* widget() { return widget_.get(); }
  const views::Widget* widget() const { return widget_.get(); }
  content::WebContents* web_contents() const;
  OmniboxEverywhereWidgetDelegate* widget_delegate();
  const OmniboxEverywhereWidgetDelegate* widget_delegate() const;

  bool IsPointInDraggableRegion(const gfx::Point& point) const;

  // For testing:
  WebUIContentsWrapper* contents_wrapper_for_testing() {
    return contents_wrapper_.get();
  }
  const std::optional<SkRegion>& draggable_region_for_testing() const {
    return draggable_region_;
  }
  bool is_file_chooser_open_for_testing() const {
    return is_file_chooser_open_;
  }
  bool is_drive_picker_open_for_testing() const {
    return is_drive_picker_open_;
  }
  bool is_screenshare_picker_open_for_testing() const {
    return is_screenshare_picker_open_;
  }
  bool is_screenshare_disclosure_open_for_testing() const {
    return is_screenshare_disclosure_open_;
  }
  views::Widget* disclosure_dialog_widget_for_testing() {
    return disclosure_dialog_widget_.get();
  }
  bool is_permission_prompt_open_for_testing() const {
    return is_permission_prompt_open_;
  }
  OmniboxEverywhereRegionSelectOverlay* region_select_overlay_for_testing() {
    return region_select_overlay_.get();
  }
  bool is_context_menu_open_for_testing() const {
    return is_context_menu_open_;
  }
  void set_is_context_menu_open_for_testing(bool open) {
    is_context_menu_open_ = open;
  }
  void OnContextMenuClosedForTesting() { OnContextMenuClosed(); }
  const ui::SimpleMenuModel* context_menu_model_for_testing() const {
    return context_menu_model_.get();
  }
  using MenuRunnerFactory = base::RepeatingCallback<std::unique_ptr<
      views::MenuRunner>(ui::MenuModel*, base::RepeatingClosure)>;
  void SetMenuRunnerFactoryForTesting(MenuRunnerFactory factory) {
    menu_runner_factory_ = std::move(factory);
  }

 private:
  void EnsureContentsWrapperInitialized(Profile* profile);
  void CreateAndInitWidget(gfx::NativeWindow context);
  void ActivateAndFocus();
  void OnEphemeralModelPrefChanged();
  void OnMostVisitedPrefChanged();
  void RecordFreImpression();
  static gfx::Rect CalculateWidgetBounds(int height);

  // Try and acquire process and profile keep alives. If unsuccessful, releases
  // keep alives (if any) and returns false.
  bool TryAcquireKeepAlives();
  // Try and acquire process and profile keep alives. Returns true if
  // successful, false otherwise.
  bool AcquireKeepAlives();
  void ReleaseKeepAlives();

  std::unique_ptr<WebUIContentsWrapper> CreateContentsWrapper(Profile* profile);

  void BuildInputContextMenu(const content::ContextMenuParams& params);
  void BuildSelectionContextMenu(const content::ContextMenuParams& params);
  void BuildBackgroundContextMenu(const content::ContextMenuParams& params);
  void AppendSettingsContextMenu();

  void CleanUpWidget();
  void OnWidgetClosed(views::Widget::ClosedReason reason);
  void OnContextMenuClosed();
  void HandleWidgetDeactivated();
  void OnScreenshotDisclosureClosed(base::OnceClosure on_cancelled,
                                    views::Widget::ClosedReason reason);

#if defined(USE_AURA)
  std::unique_ptr<OmniboxEverywhereEventHandlerAura> event_handler_;
#endif

  // The native window hosting the Omnibox Everywhere UI.
  raw_ptr<Profile> profile_ = nullptr;
  ContentsWrapperFactory contents_wrapper_factory_;
  MenuRunnerFactory menu_runner_factory_;

  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
  std::unique_ptr<OmniboxEverywhereWidgetDelegate> widget_delegate_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<OmniboxEverywhereRegionSelectOverlay> region_select_overlay_;

  std::unique_ptr<views::Widget> disclosure_dialog_widget_;

  bool is_file_chooser_open_ = false;
  bool is_drive_picker_open_ = false;
  bool is_context_menu_open_ = false;
  bool is_demoted_ = false;
  bool is_screenshare_picker_open_ = false;
  bool is_screenshare_disclosure_open_ = false;
  bool is_permission_prompt_open_ = false;
  bool is_dragging_ = false;
  std::optional<gfx::Size> pending_auto_resize_size_;
  std::optional<SkRegion> draggable_region_;

  std::unique_ptr<views::UnhandledKeyboardEventHandler>
      unhandled_keyboard_event_handler_;
  content::ContextMenuParams last_context_menu_params_;
  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  std::optional<base::TimeTicks> last_shown_time_;
  // Task posted when the widget is deactivated, used to either dismiss or
  // reactivate the widget after the grace period.
  base::CancelableOnceClosure deactivation_task_;

  PrefChangeRegistrar local_state_pref_change_registrar_;
  PrefChangeRegistrar profile_pref_change_registrar_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  base::ScopedObservation<PermissionPromptObserver,
                          PermissionPromptObserver::Observer>
      permission_prompt_observation_{this};

  base::WeakPtrFactory<OmniboxEverywhereUIManager> weak_factory_{this};
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_MANAGER_H_
