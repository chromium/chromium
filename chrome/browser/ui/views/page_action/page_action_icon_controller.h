// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTROLLER_H_

#include "base/macros.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/zoom/zoom_event_manager.h"
#include "components/zoom/zoom_event_manager_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/view.h"

class CookieControlsIconView;
class FindBarIcon;
class IntentPickerView;
class ManagePasswordsIconViews;
class FileSystemAccessIconView;
class PageActionIconContainer;
struct PageActionIconParams;
class PwaInstallView;
class ReaderModeIconView;
class SharingIconView;
class StarView;
class TranslateIconView;
class WebAuthnIconView;
class ZoomView;

namespace autofill {
class LocalCardMigrationIconView;
class OfferNotificationIconView;
class SaveAddressProfileIconView;
class SavePaymentIconView;
}  // namespace autofill

namespace qrcode_generator {
class QRCodeGeneratorIconView;
}

namespace send_tab_to_self {
class SendTabToSelfIconView;
}

class PageActionIconController : public zoom::ZoomEventManagerObserver {
 public:
  PageActionIconController();
  ~PageActionIconController() override;

  void Init(const PageActionIconParams& params,
            PageActionIconContainer* icon_container);

  PageActionIconView* GetIconView(PageActionIconType type);

  // Updates the visual state of all enabled page action icons.
  void UpdateAll();

  bool IsAnyIconVisible() const;

  // Activates the first visible but inactive icon for accessibility. Returns
  // whether any icons were activated.
  bool ActivateFirstInactiveBubbleForAccessibility();

  // Update the icons' color.
  void SetIconColor(SkColor icon_color);

  // Update the icons' fonts.
  void SetFontList(const gfx::FontList& font_list);

  // See comment in browser_window.h for more info.
  void ZoomChangedForActiveTab(bool can_show_bubble);

  std::vector<const PageActionIconView*> GetPageActionIconViewsForTesting()
      const;

 private:
  // ZoomEventManagerObserver:
  // Updates the view for the zoom icon when default zoom levels change.
  void OnDefaultZoomLevelChanged() override;

  PageActionIconContainer* icon_container_ = nullptr;

  StarView* bookmark_star_icon_ = nullptr;
  SharingIconView* click_to_call_icon_ = nullptr;
  CookieControlsIconView* cookie_controls_icon_ = nullptr;
  FindBarIcon* find_icon_ = nullptr;
  IntentPickerView* intent_picker_icon_ = nullptr;
  autofill::LocalCardMigrationIconView* local_card_migration_icon_ = nullptr;
  ManagePasswordsIconViews* manage_passwords_icon_ = nullptr;
  FileSystemAccessIconView* file_system_access_icon_ = nullptr;
  autofill::OfferNotificationIconView* offer_notification_icon_ = nullptr;
  PwaInstallView* pwa_install_icon_ = nullptr;
  qrcode_generator::QRCodeGeneratorIconView* qrcode_generator_icon_view_ =
      nullptr;
  ReaderModeIconView* reader_mode_icon_ = nullptr;
  autofill::SaveAddressProfileIconView* save_autofill_address_icon_ = nullptr;
  autofill::SavePaymentIconView* save_payment_icon_ = nullptr;
  send_tab_to_self::SendTabToSelfIconView* send_tab_to_self_icon_ = nullptr;
  SharingIconView* shared_clipboard_icon_ = nullptr;
  SharingIconView* sms_remote_fetcher_icon_ = nullptr;
  TranslateIconView* translate_icon_ = nullptr;
  WebAuthnIconView* webauthn_icon_ = nullptr;
  ZoomView* zoom_icon_ = nullptr;

  std::vector<PageActionIconView*> page_action_icons_;

  base::ScopedObservation<zoom::ZoomEventManager,
                          zoom::ZoomEventManagerObserver>
      zoom_observation_{this};

  DISALLOW_COPY_AND_ASSIGN(PageActionIconController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTROLLER_H_
