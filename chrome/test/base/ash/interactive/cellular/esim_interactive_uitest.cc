// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/ash_element_identifiers.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ash/interactive/cellular/cellular_util.h"
#include "chrome/test/base/ash/interactive/cellular/esim_interactive_uitest_base.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/ash/interactive/network/shill_device_power_state_observer.h"
#include "chrome/test/base/ash/interactive/settings/interactive_uitest_elements.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/polling_view_observer.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using EsimInteractiveUiTest = EsimInteractiveUiTestBase;

IN_PROC_BROWSER_TEST_F(EsimInteractiveUiTest,
                       OpenAddEsimDialogFromQuickSettings) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOSSettingsId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ShillDevicePowerStateObserver,
                                      kMobileDataPoweredState);

  using Observer = views::test::PollingViewObserver<bool, views::View>;
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(Observer, kPollingViewState);

  bool has_clicked_add_esim_entry = false;

  RunTestSequence(
      Log("Waiting for cellular to be enabled"),

      ObserveState(
          kMobileDataPoweredState,
          std::make_unique<ShillDevicePowerStateObserver>(
              ShillManagerClient::Get(), NetworkTypePattern::Mobile())),
      WaitForState(kMobileDataPoweredState, true),

      Log("Opening Quick Settings and navigating to the network page"),

      OpenQuickSettings(), NavigateQuickSettingsToNetworkPage(),

      Log("Waiting for the 'add eSIM' button to be visible, then clicking it"),

      InstrumentNextTab(kOSSettingsId, AnyBrowser()),

      // The views in the network page of Quick Settings (that are not
      // top-level e.g. the toggles or headers) are prone to frequent
      // re-ordering and/or can rapidly appear/disappear before being stable due
      // to network state changes. Instead of attempting to click the view via
      // moving and clicking the mouse we instead click via code to avoid the
      // possibility of the element disappearing during the step.
      PollView(
          kPollingViewState, kNetworkAddEsimElementId,
          [&has_clicked_add_esim_entry](const views::View* view) -> bool {
            if (!has_clicked_add_esim_entry) {
              views::test::ButtonTestApi(
                  views::Button::AsButton(const_cast<views::View*>(view)))
                  .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed,
                                              gfx::PointF(), gfx::PointF(),
                                              base::TimeTicks(), 0, 0));
            }
            has_clicked_add_esim_entry = true;
            return true;
          },
          base::Milliseconds(50)),

      // The `WaitForState` step also requires that the element in question
      // exists for the duration of the step. As mentioned above, the element
      // may rapidly appear/disappear which would cause `WaitForState` to fail.
      // Instead, we wait for the Quick Settings to close as a result of the
      // button being clicked.

      WaitForHide(ash::kQuickSettingsViewElementId),

      Log("Waiting for OS Settings to open"),

      InAnyContext(WaitForShow(kOSSettingsId)));

  ui::ElementContext context = FindSystemWebApp(SystemWebAppType::SETTINGS);

  // Run the remaining steps with a longer timeout since it can take more than
  // 10 seconds for OS Settings to open.
  const base::test::ScopedRunLoopTimeout longer_timeout(FROM_HERE,
                                                        base::Seconds(15));

  // Run the following steps with the OS Settings context set as the default.
  RunTestSequenceInContext(
      context,

      Log("Waiting for OS Settings to navigate to cellular subpage"),

      WaitForElementTextContains(
          kOSSettingsId, settings::SettingsSubpageTitle(),
          /*text=*/l10n_util::GetStringUTF8(IDS_NETWORK_TYPE_MOBILE_DATA)),

      Log("Waiting for 'add eSIM' dialog to open"),

      WaitForElementTextContains(
          kOSSettingsId, settings::cellular::EsimDialogTitle(),
          /*text=*/
          l10n_util::GetStringUTF8(
              IDS_CELLULAR_SETUP_ESIM_PAGE_PROFILE_DISCOVERY_CONSENT_TITLE)),

      Do([&]() { CloseSystemWebApp(SystemWebAppType::SETTINGS); }),

      Log("Test complete"));
}

}  // namespace
}  // namespace ash
