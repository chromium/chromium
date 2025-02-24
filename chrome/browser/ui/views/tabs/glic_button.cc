// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic_button.h"

#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_vector_icon_manager.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

namespace glic {

#if BUILDFLAG(ENABLE_GLIC)
class GlicButton::GlicPanelStateObserver
    : public GlicWindowController::StateObserver {
 public:
  GlicPanelStateObserver(glic::GlicButton* glic_button,
                         glic::GlicWindowController* glic_window_controller)
      : glic_button_(glic_button),
        glic_window_controller_(glic_window_controller) {
    glic_window_controller_->AddStateObserver(this);
    PanelStateChanged(glic_window_controller_->GetPanelState(), nullptr);
  }

  void PanelStateChanged(const mojom::PanelState& panel_state,
                         Browser*) override {
    UpdateIconToState(panel_state);
  }

  ~GlicPanelStateObserver() override {
    glic_window_controller_->RemoveStateObserver(this);
  }

 private:
  void UpdateIconToState(const mojom::PanelState& panel_state) {
    if (panel_state.kind == mojom::PanelState_Kind::kHidden) {
      glic_button_->SetVectorIcon(
          GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON));
    } else {
      glic_button_->SetVectorIcon(GlicVectorIconManager::GetVectorIcon(
          IDR_GLIC_ATTACH_BUTTON_VECTOR_ICON));
    }
  }

  raw_ptr<glic::GlicButton> glic_button_;
  raw_ptr<glic::GlicWindowController> glic_window_controller_;
};
#endif

GlicButton::GlicButton(TabStripController* tab_strip_controller)
    : TabStripControlButton(
          tab_strip_controller,
          PressedCallback(base::BindRepeating(&GlicButton::ToggleUI,
                                              base::Unretained(this))),
#if BUILDFLAG(ENABLE_GLIC)
          GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON)
#else
          gfx::VectorIcon::EmptyIcon()
#endif
      ) {
  tab_strip_controller_ = tab_strip_controller;
  SetProperty(views::kElementIdentifierKey, kGlicButtonElementId);

#if BUILDFLAG(ENABLE_GLIC)
  SetTooltipText(l10n_util::GetStringUTF16(IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP));
#endif
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_SEARCH));

  SetForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);
  SetForegroundFrameInactiveColorId(kColorNewTabButtonForegroundFrameInactive);
  SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorNewTabButtonCRBackgroundFrameInactive);

  UpdateColors();

#if BUILDFLAG(ENABLE_GLIC)
  GlicKeyedService* glic_keyed_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(
          tab_strip_controller_->GetProfile());
  glic_keyed_service->TryPreload();

  glic_panel_state_observer_ = std::make_unique<GlicPanelStateObserver>(
      this, &glic_keyed_service->window_controller());
#endif  // BUILDFLAG(ENABLE_GLIC)
}

GlicButton::~GlicButton() = default;

void GlicButton::ToggleUI() {
  // Indicate that the glic button was pressed so that we can either close the
  // IPH promo (if present) or note that it has already been used to prevent
  // unnecessarily displaying the promo.
  tab_strip_controller_->GetBrowserWindowInterface()
      ->GetUserEducationInterface()
      ->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHGlicPromoFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);

#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      tab_strip_controller_->GetProfile())
      ->ToggleUI(tab_strip_controller_->GetBrowserWindowInterface());
#endif  // BUILDFLAG(ENABLE_GLIC)
}

void GlicButton::SetDropToAttachIndicator(bool indicate) {
  if (indicate) {
    SetBackgroundFrameActiveColorId(ui::kColorSysStateHeaderHover);
  } else {
    SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  }
}

gfx::Rect GlicButton::GetBoundsWithInset() const {
  gfx::Rect bounds = GetBoundsInScreen();
  bounds.Inset(GetInsets());
  return bounds;
}

BEGIN_METADATA(GlicButton)
END_METADATA

}  // namespace glic
