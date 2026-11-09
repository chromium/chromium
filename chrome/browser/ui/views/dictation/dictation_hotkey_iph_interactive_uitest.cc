// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <tuple>

#include "base/time/time.h"
#include "chrome/browser/dictation/dictation_browser_test_base.h"
#include "chrome/browser/dictation/dictation_multiplexer.h"
#include "chrome/browser/dictation/hotkey_manager.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/session_controller.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/dictation/dictation_iph.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/browser/global_dom_node_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/dom/dom_node_id.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/label.h"

namespace dictation {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

using ExtensionStreamState = extensions::api::dictation_private::StreamState;

// Page with a text input, a textarea and a contenteditable element.
constexpr char kPagePath[] = "/textinput/all_input_node.html";

// CSS selectors for fields on that page.
constexpr char kTextInput[] = "#text_id";
constexpr char kTextarea[] = "#textarea_id";

}  // namespace

// `DictationBrowserTestBase` marks dictation onboarding complete, sets up the
// dictation hotkey, and installs a test connector extension, so the tests run
// real dictation sessions.
class DictationHotkeyIphUiTest
    : public InteractiveFeaturePromoTestMixin<DictationBrowserTestBase> {
 public:
  DictationHotkeyIphUiTest()
      : InteractiveFeaturePromoTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHDictationHotkeyFeature})) {}
  ~DictationHotkeyIphUiTest() override = default;

 protected:
  // Opens the test page in the active tab.
  auto OpenTestPage() {
    return Steps(
        InstrumentTab(kTabId),
        NavigateWebContents(kTabId, embedded_test_server()->GetURL(kPagePath)));
  }

  // Focuses the field matching `selector` with the mouse, as a user would.
  auto ClickField(const char* selector) {
    return Steps(MoveMouseTo(kTabId, DeepQuery{selector}), ClickMouse());
  }

  // Returns the caret bounds in the main frame, in screen coordinates, if the
  // browser knows them.
  std::optional<gfx::Rect> GetCaretBounds() {
    content::WebContents* const web_contents =
        chrome_test_utils::GetActiveWebContents(this);
    return web_contents->GetFocusSelectionBounds(
        web_contents->GetPrimaryMainFrame());
  }

  // Clicks the textarea and waits until the IPH anchor is on it and the browser
  // knows where its caret is.
  auto ClickTextarea() {
    return Steps(ClickField(kTextarea),
                 WaitForShow(kDictationIphAnchorElementId),
                 PollUntil([this]() { return GetCaretBounds().has_value(); },
                           "Wait for the caret."));
  }

  // Waits until the focused field's text input type has reached the browser,
  // which is when the controller knows whether the field is medium/large.
  auto WaitForFieldType() {
    return PollUntil(
        [this]() {
          content::RenderFrameHost* const frame =
              chrome_test_utils::GetActiveWebContents(this)->GetFocusedFrame();
          return frame && frame->GetFocusedElementTextInputType().has_value();
        },
        "Wait for the focused field's text input type.");
  }

  // Starts a real dictation session on the field matching `selector`, as if the
  // user invoked dictation on it, then stops dictating and waits for the
  // session to end.
  auto RunSessionIn(const char* selector) {
    return Steps(Do([this, selector = std::string(selector)]() {
                   content::RenderFrameHost* const frame =
                       chrome_test_utils::GetActiveWebContents(this)
                           ->GetPrimaryMainFrame();
                   StartSession(TargetDetails(content::GlobalDOMNodeId{
                       frame->GetWeakDocumentPtr(),
                       blink::DOMNodeIdType(
                           content::GetDOMNodeId(*frame, selector).value())}));
                   ASSERT_NE(attached_stream(), nullptr);
                   const DictationMultiplexer::StreamId stream_id =
                       attached_stream()->stream_id_for_testing();
                   ExtensionWaitForStreamStart(profile(), stream_id);
                   ExtensionSendStreamStateUpdate(
                       profile(), stream_id,
                       ExtensionStreamState::kTranscribing);
                   session_controller()->UiRequestEndActiveStream();
                   ExtensionSendStreamStateUpdate(
                       profile(), stream_id, ExtensionStreamState::kComplete);
                 }),
                 PollUntil([this]() { return session_controller() == nullptr; },
                           "Wait for the session to end."));
  }

  // Opens the test page, clicks the textarea and invokes dictation on it twice.
  auto InvokeTwiceInTextarea() {
    return Steps(OpenTestPage(), ClickTextarea(), RunSessionIn(kTextarea),
                 RunSessionIn(kTextarea));
  }

  // Invokes dictation on the textarea twice, which shows the IPH.
  auto ShowPromo() {
    return Steps(InvokeTwiceInTextarea(),
                 WaitForPromo(feature_engagement::kIPHDictationHotkeyFeature));
  }

  auto CheckPromoNotRequested() {
    return CheckPromoRequested(feature_engagement::kIPHDictationHotkeyFeature,
                               false);
  }

  auto WaitForPromoToClose() {
    return Steps(
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        CheckPromoNotRequested());
  }

  user_education::UserEducationStorageService& storage() {
    return UserEducationServiceFactory::GetForBrowserContext(profile())
        ->user_education_storage_service();
  }

  // Checks the hotkey IPH's stored data.
  auto CheckPromoData(bool is_dismissed, int show_count, int snooze_count) {
    return CheckResult(
        [this]() {
          const std::optional<user_education::FeaturePromoData> data =
              storage().ReadPromoData(
                  feature_engagement::kIPHDictationHotkeyFeature);
          return data ? std::make_tuple(data->is_dismissed, data->show_count,
                                        data->snooze_count)
                      : std::make_tuple(false, 0, 0);
        },
        std::make_tuple(is_dismissed, show_count, snooze_count));
  }

  // Stores data for a hotkey IPH that was shown `show_count` times, of which
  // `snooze_count` were snoozed, last `ago` before now.
  auto SetPromoData(int show_count, int snooze_count, base::TimeDelta ago) {
    return Do([this, show_count, snooze_count, ago]() {
      user_education::FeaturePromoData data;
      data.show_count = show_count;
      data.snooze_count = snooze_count;
      data.first_show_time = storage().GetCurrentTime() - ago;
      data.last_show_time = data.first_show_time;
      if (snooze_count > 0) {
        data.last_snooze_time = data.last_show_time;
      }
      storage().SavePromoData(feature_engagement::kIPHDictationHotkeyFeature,
                              data);
    });
  }

  ui::Accelerator hotkey() { return GetDictationHotkeyFromPrefs(profile()); }
};

IN_PROC_BROWSER_TEST_F(DictationHotkeyIphUiTest, ShowsAfterSecondInvocation) {
  // clang-format off
  RunTestSequence(
      OpenTestPage(),
      ClickTextarea(),
      RunSessionIn(kTextarea),
      CheckPromoNotRequested(),
      RunSessionIn(kTextarea),
      WaitForPromo(feature_engagement::kIPHDictationHotkeyFeature),
      // The body tells the user which hotkey to press.
      CheckViewProperty(
          user_education::HelpBubbleView::kBodyTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetStringFUTF16(IDS_DICTATION_HOTKEY_IPH_BODY,
                                     hotkey().GetShortcutText())));
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationHotkeyIphUiTest, DoesNotShowOnSmallField) {
  // clang-format off
  RunTestSequence(
      OpenTestPage(),
      ClickField(kTextInput),
      WaitForFieldType(),
      // The anchor is removed from fields that aren't medium/large.
      WaitForHide(kDictationIphAnchorElementId),
      RunSessionIn(kTextInput),
      RunSessionIn(kTextInput),
      CheckPromoNotRequested());
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationHotkeyIphUiTest,
                       DoesNotShowForSessionInAnotherField) {
  // clang-format off
  RunTestSequence(
      OpenTestPage(),
      ClickTextarea(),
      RunSessionIn(kTextInput),
      RunSessionIn(kTextInput),
      CheckPromoNotRequested());
  // clang-format on
}

// The anchor only moves when focus changes, so it no longer matches the
// textarea once the page moves it.
IN_PROC_BROWSER_TEST_F(DictationHotkeyIphUiTest, DoesNotShowWhenFieldMoved) {
  std::optional<gfx::Rect> caret_before_move;
  // clang-format off
  RunTestSequence(
      OpenTestPage(),
      ClickTextarea(),
      Do([this, &caret_before_move]() {
        caret_before_move = GetCaretBounds();
      }),
      ExecuteJs(kTabId, R"(() => {
        document.getElementById('textarea_id').style.marginTop = '200px';
      })"),
      PollUntil(
          [this, &caret_before_move]() {
            const std::optional<gfx::Rect> caret = GetCaretBounds();
            return caret && caret->y() >= caret_before_move->y() + 100;
          },
          "Wait for the caret to move with the textarea."),
      RunSessionIn(kTextarea),
      RunSessionIn(kTextarea),
      CheckPromoNotRequested());
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationHotkeyIphUiTest, DoesNotShowWithoutHotkey) {
  profile()->GetPrefs()->SetString(prefs::kVoiceTypingHotkey, "");
  // clang-format off
  RunTestSequence(
      InvokeTwiceInTextarea(),
      CheckPromoNotRequested());
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationHotkeyIphUiTest, HotkeyClosesPromo) {
  // clang-format off
  RunTestSequence(
      ShowPromo(),
      SendAccelerator(kBrowserViewElementId, hotkey()),
      WaitForPromoToClose(),
      // Using the hotkey dismisses the IPH for good.
      CheckPromoData(/*is_dismissed=*/true, /*show_count=*/1,
                     /*snooze_count=*/0));
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationHotkeyIphUiTest, StopsAfterMaxShows) {
  // clang-format off
  RunTestSequence(
      SetPromoData(kMaxDictationHotkeyIphShows,
                   /*snooze_count=*/kMaxDictationHotkeyIphShows - 1,
                   base::Days(30)),
      InvokeTwiceInTextarea(),
      CheckPromoNotRequested());
  // clang-format on
}

}  // namespace dictation
