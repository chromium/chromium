// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_FEATURES_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_FEATURES_H_

#include <memory>

#include "base/functional/callback.h"
#include "chrome/common/buildflags.h"

#if BUILDFLAG(ENABLE_GLIC)
namespace glic {
class GlicButtonController;
class GlicIphController;
}  // namespace glic
#endif

class Browser;
class BrowserInstantController;
class BrowserView;
class BrowserWindowInterface;
class ChromeLabsCoordinator;
class CookieControlsBubbleCoordinator;
class HistorySidePanelCoordinator;
class BookmarksSidePanelCoordinator;
class MemorySaverOptInIPHController;
class SidePanelCoordinator;
class SidePanelUI;
class TabMenuModelDelegate;
class TabSearchToolbarButtonController;
class TabStripModel;
class TranslateBubbleController;
class ToastController;
class ToastService;
class DownloadToolbarUIController;
class TabStripServiceRegister;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
class PdfInfoBarController;
#endif

namespace extensions {
class ExtensionSidePanelManager;
class Mv2DisabledDialogController;
}  // namespace extensions

namespace tabs {
class TabDeclutterController;
}  // namespace tabs

namespace commerce {
class ProductSpecificationsEntryPointController;
}  // namespace commerce

namespace tabs {
class GlicNudgeController;
}

namespace lens {
class LensOverlayEntryPointController;
class LensRegionSearchController;
}  // namespace lens

namespace media_router {
class CastBrowserController;
}  // namespace media_router

namespace memory_saver {
class MemorySaverBubbleController;
}  // namespace memory_saver

namespace new_tab_footer {
class NewTabFooterController;
}  // namespace new_tab_footer

namespace tab_groups {
class SessionServiceTabGroupSyncObserver;
class SharedTabGroupFeedbackController;
class MostRecentSharedTabUpdateStore;
}  // namespace tab_groups

namespace send_tab_to_self {
class SendTabToSelfToolbarBubbleController;
}  // namespace send_tab_to_self

// This class owns the core controllers for features that are scoped to a given
// browser window on desktop. It can be subclassed by tests to perform
// dependency injection.
class BrowserWindowFeatures {
 public:
  static std::unique_ptr<BrowserWindowFeatures> CreateBrowserWindowFeatures();
  virtual ~BrowserWindowFeatures();

  BrowserWindowFeatures(const BrowserWindowFeatures&) = delete;
  BrowserWindowFeatures& operator=(const BrowserWindowFeatures&) = delete;

  // Call this method to stub out BrowserWindowFeatures for tests.
  using BrowserWindowFeaturesFactory =
      base::RepeatingCallback<std::unique_ptr<BrowserWindowFeatures>()>;
  static void ReplaceBrowserWindowFeaturesForTesting(
      BrowserWindowFeaturesFactory factory);

  // Called exactly once to initialize features. This is called prior to
  // instantiating BrowserView, to allow the view hierarchy to depend on state
  // in this class.
  void Init(BrowserWindowInterface* browser);

  // Called exactly once to initialize features that depend on the window object
  // being created.
  void InitPostWindowConstruction(Browser* browser);

  // Called exactly once to initialize features that depend on the view
  // hierarchy in BrowserView.
  void InitPostBrowserViewConstruction(BrowserView* browser_view);

  // Called exactly once to tear down state that depends on BrowserView.
  void TearDownPreBrowserViewDestruction();

  // Public accessors for features:
  commerce::ProductSpecificationsEntryPointController*
  product_specifications_entry_point_controller() {
    return product_specifications_entry_point_controller_.get();
  }
  extensions::Mv2DisabledDialogController*
  mv2_disabled_dialog_controller_for_testing() {
    return mv2_disabled_dialog_controller_.get();
  }

  ChromeLabsCoordinator* chrome_labs_coordinator() {
    return chrome_labs_coordinator_.get();
  }

  media_router::CastBrowserController* cast_browser_controller() {
    return cast_browser_controller_.get();
  }

  HistorySidePanelCoordinator* history_side_panel_coordinator() {
    return history_side_panel_coordinator_.get();
  }

  BookmarksSidePanelCoordinator* bookmarks_side_panel_coordinator() {
    return bookmarks_side_panel_coordinator_.get();
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  PdfInfoBarController* pdf_infobar_controller() {
    return pdf_infobar_controller_.get();
  }
#endif

  // TODO(crbug.com/346158959): For historical reasons, side_panel_ui is an
  // abstract base class that contains some, but not all of the public interface
  // of SidePanelCoordinator. One of the accessors side_panel_ui() or
  // side_panel_coordinator() should be removed. For consistency with the rest
  // of this class, we use lowercase_with_underscores even though the
  // implementation is not inlined.
  SidePanelUI* side_panel_ui();

  SidePanelCoordinator* side_panel_coordinator() {
    return side_panel_coordinator_.get();
  }

  lens::LensOverlayEntryPointController* lens_overlay_entry_point_controller() {
    return lens_overlay_entry_point_controller_.get();
  }

  lens::LensRegionSearchController* lens_region_search_controller() {
    return lens_region_search_controller_.get();
  }

  tabs::TabDeclutterController* tab_declutter_controller() {
    return tab_declutter_controller_.get();
  }

  tabs::GlicNudgeController* glic_nudge_controller() {
    return glic_nudge_controller_.get();
  }

  TabStripModel* tab_strip_model() { return tab_strip_model_; }

  // Returns a pointer to the ToastController for the browser window. This can
  // return nullptr for non-normal browser windows because toasts are not
  // supported for those cases.
  ToastController* toast_controller();

  // Returns a pointer to the ToastService for the browser window. This can
  // return nullptr for non-normal browser windows because toasts are not
  // supported for those cases.
  ToastService* toast_service() { return toast_service_.get(); }

  send_tab_to_self::SendTabToSelfToolbarBubbleController*
  send_tab_to_self_toolbar_bubble_controller() {
    return send_tab_to_self_toolbar_bubble_controller_.get();
  }

  extensions::ExtensionSidePanelManager* extension_side_panel_manager() {
    return extension_side_panel_manager_.get();
  }

  DownloadToolbarUIController* download_toolbar_ui_controller() {
    return download_toolbar_ui_controller_.get();
  }

  tab_groups::MostRecentSharedTabUpdateStore*
  most_recent_shared_tab_update_store() {
    return most_recent_shared_tab_update_store_.get();
  }

  memory_saver::MemorySaverBubbleController* memory_saver_bubble_controller() {
    return memory_saver_bubble_controller_.get();
  }

  tab_groups::SharedTabGroupFeedbackController*
  shared_tab_group_feedback_controller() {
    return shared_tab_group_feedback_controller_.get();
  }

  TranslateBubbleController* translate_bubble_controller() {
    return translate_bubble_controller_.get();
  }

  TabSearchToolbarButtonController* tab_search_toolbar_button_controller() {
    return tab_search_toolbar_button_controller_.get();
  }

  CookieControlsBubbleCoordinator* cookie_controls_bubble_coordinator() {
    return cookie_controls_bubble_coordinator_.get();
  }

  TabMenuModelDelegate* tab_menu_model_delegate() {
    return tab_menu_model_delegate_.get();
  }

  // Only fetch the tab_strip_service to register a pending receiver.
  TabStripServiceRegister* tab_strip_service() {
    return tab_strip_service_.get();
  }

  new_tab_footer::NewTabFooterController* new_tab_footer_controller() {
    return new_tab_footer_controller_.get();
  }

 protected:
  BrowserWindowFeatures();

  // Override these methods to stub out individual feature controllers for
  // testing. e.g.
  // virtual std::unique_ptr<FooFeature> CreateFooFeature();

 private:
  // Features that are per-browser window will each have a controller. e.g.
  // std::unique_ptr<FooFeature> foo_feature_;

  std::unique_ptr<BrowserInstantController> instant_controller_;

  std::unique_ptr<send_tab_to_self::SendTabToSelfToolbarBubbleController>
      send_tab_to_self_toolbar_bubble_controller_;

  std::unique_ptr<ChromeLabsCoordinator> chrome_labs_coordinator_;

  std::unique_ptr<commerce::ProductSpecificationsEntryPointController>
      product_specifications_entry_point_controller_;

  std::unique_ptr<lens::LensOverlayEntryPointController>
      lens_overlay_entry_point_controller_;

  std::unique_ptr<lens::LensRegionSearchController>
      lens_region_search_controller_;

  std::unique_ptr<extensions::Mv2DisabledDialogController>
      mv2_disabled_dialog_controller_;

  std::unique_ptr<tabs::TabDeclutterController> tab_declutter_controller_;

  std::unique_ptr<MemorySaverOptInIPHController>
      memory_saver_opt_in_iph_controller_;

  std::unique_ptr<HistorySidePanelCoordinator> history_side_panel_coordinator_;

  std::unique_ptr<BookmarksSidePanelCoordinator>
      bookmarks_side_panel_coordinator_;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  std::unique_ptr<PdfInfoBarController> pdf_infobar_controller_;
#endif

  std::unique_ptr<SidePanelCoordinator> side_panel_coordinator_;

  std::unique_ptr<tab_groups::SessionServiceTabGroupSyncObserver>
      session_service_tab_group_sync_observer_;

  raw_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<ToastService> toast_service_;

  // The window-scoped extension side-panel manager. There is a separate
  // tab-scoped extension side-panel manager.
  std::unique_ptr<extensions::ExtensionSidePanelManager>
      extension_side_panel_manager_;

  std::unique_ptr<media_router::CastBrowserController> cast_browser_controller_;

  std::unique_ptr<DownloadToolbarUIController> download_toolbar_ui_controller_;

  std::unique_ptr<tabs::GlicNudgeController> glic_nudge_controller_;

#if BUILDFLAG(ENABLE_GLIC)
  std::unique_ptr<glic::GlicButtonController> glic_button_controller_;
  std::unique_ptr<glic::GlicIphController> glic_iph_controller_;
#endif

  std::unique_ptr<tab_groups::MostRecentSharedTabUpdateStore>
      most_recent_shared_tab_update_store_;

  std::unique_ptr<memory_saver::MemorySaverBubbleController>
      memory_saver_bubble_controller_;

  std::unique_ptr<tab_groups::SharedTabGroupFeedbackController>
      shared_tab_group_feedback_controller_;

  std::unique_ptr<TranslateBubbleController> translate_bubble_controller_;

  std::unique_ptr<TabSearchToolbarButtonController>
      tab_search_toolbar_button_controller_;

  std::unique_ptr<CookieControlsBubbleCoordinator>
      cookie_controls_bubble_coordinator_;

  std::unique_ptr<TabMenuModelDelegate> tab_menu_model_delegate_;

  std::unique_ptr<new_tab_footer::NewTabFooterController>
      new_tab_footer_controller_;

  // This is an experimental API that interacts with the TabStripModel.
  std::unique_ptr<TabStripServiceRegister> tab_strip_service_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_FEATURES_H_
