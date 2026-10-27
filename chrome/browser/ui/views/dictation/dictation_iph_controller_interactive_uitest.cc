// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/dictation/dictation_iph.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

namespace dictation {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);

// Page with a text input, a textarea and a contenteditable element.
constexpr char kPagePath[] = "/textinput/all_input_node.html";

// CSS selectors for the fields on that page.
constexpr char kTextInput[] = "#text_id";
constexpr char kTextarea[] = "#textarea_id";
constexpr char kContentEditable[] = "#contenteditable_id";

// CSS selector for the iframe added by `AddIframeWithTextarea()`.
constexpr char kIframe[] = "#frame";

}  // namespace

class DictationIphControllerUiTest : public InteractiveFeaturePromoTest {
 public:
  DictationIphControllerUiTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHDictationFeature})) {
    // The controller is only created when dictation is enabled.
    scoped_feature_list_.InitAndEnableFeature(kDictation);
  }
  ~DictationIphControllerUiTest() override = default;

  void SetUpOnMainThread() override {
    // Lets cross-site URLs (e.g. for iframes) load from the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    InteractiveFeaturePromoTest::SetUpOnMainThread();

    MediaCaptureDevicesDispatcher::GetInstance()
        ->DisableDeviceEnumerationForTesting();
    MediaCaptureDevicesDispatcher::GetInstance()->SetTestAudioCaptureDevices(
        {blink::MediaStreamDevice(
            blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "fake_mic",
            "Fake Microphone")});
  }

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

  // Waits until the focused field's text input type has reached the browser,
  // which is when the controller can decide whether the field qualifies.
  auto WaitForFieldType() {
    return PollUntil(
        [this]() {
          content::RenderFrameHost* const frame =
              chrome_test_utils::GetActiveWebContents(this)->GetFocusedFrame();
          return frame && frame->GetFocusedElementTextInputType().has_value();
        },
        "Wait for the focused field's text input type.");
  }

  // Clicks the textarea and waits for the IPH to show.
  auto ShowPromoOnTextarea() {
    return Steps(OpenTestPage(), ClickField(kTextarea),
                 WaitForPromo(feature_engagement::kIPHDictationFeature));
  }

  // Replaces the test page's content with a same-origin iframe holding a
  // textarea, and waits for the iframe to load.
  auto AddIframeWithTextarea() {
    return CheckJsResult(kTabId, R"(
        () => new Promise(resolve => {
          const frame = document.createElement('iframe');
          frame.id = 'frame';
          frame.style.width = '600px';
          frame.style.height = '400px';
          frame.onload = () => resolve(true);
          frame.srcdoc = '<textarea id="textarea_id"></textarea>';
          document.body.replaceChildren(frame);
        }))");
  }

  // Focuses the field matching `selector` in the iframe with the mouse.
  auto ClickFieldInIframe(const char* selector) {
    return Steps(MoveMouseTo(kTabId, DeepQuery{kIframe, selector}),
                 ClickMouse());
  }

  // Clicks the textarea in the iframe and waits for the IPH to show.
  auto ShowPromoOnTextareaInIframe() {
    return Steps(OpenTestPage(), AddIframeWithTextarea(),
                 ClickFieldInIframe(kTextarea),
                 WaitForPromo(feature_engagement::kIPHDictationFeature));
  }

  auto WaitForPromoToClose() {
    return Steps(
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        CheckPromoRequested(feature_engagement::kIPHDictationFeature, false));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationIphControllerUiTest, ShowsOnTextarea) {
  RunTestSequence(ShowPromoOnTextarea());
}

IN_PROC_BROWSER_TEST_F(DictationIphControllerUiTest, ShowsOnContentEditable) {
  RunTestSequence(
      OpenTestPage(),
      // Give the empty element some size so it can be clicked.
      ExecuteJs(kTabId,
                "() => { document.getElementById('contenteditable_id')"
                ".textContent = 'Some text'; }"),
      ClickField(kContentEditable),
      WaitForPromo(feature_engagement::kIPHDictationFeature));
}

IN_PROC_BROWSER_TEST_F(DictationIphControllerUiTest, ShowsOnTextareaInIframe) {
  RunTestSequence(ShowPromoOnTextareaInIframe());
}

IN_PROC_BROWSER_TEST_F(DictationIphControllerUiTest, DoesNotShowOnTextInput) {
  RunTestSequence(
      OpenTestPage(), ClickField(kTextInput), WaitForFieldType(),
      // The anchor is removed as soon as the field is found not to qualify.
      WaitForHide(kDictationIphAnchorElementId),
      CheckPromoRequested(feature_engagement::kIPHDictationFeature, false));
}

IN_PROC_BROWSER_TEST_F(DictationIphControllerUiTest,
                       DoesNotShowWhenOnboardingCompleted) {
  chrome_test_utils::GetProfile(this)->GetPrefs()->SetBoolean(
      prefs::kPrefDictationOnboardingCompleted, true);
  RunTestSequence(
      OpenTestPage(), ClickField(kTextarea),
      WaitForShow(kDictationIphAnchorElementId), WaitForFieldType(),
      CheckPromoRequested(feature_engagement::kIPHDictationFeature, false));
}

IN_PROC_BROWSER_TEST_F(DictationIphControllerUiTest,
                       DoesNotShowWithoutMicrophone) {
  MediaCaptureDevicesDispatcher::GetInstance()->SetTestAudioCaptureDevices({});
  RunTestSequence(
      OpenTestPage(), ClickField(kTextarea),
      WaitForShow(kDictationIphAnchorElementId), WaitForFieldType(),
      CheckPromoRequested(feature_engagement::kIPHDictationFeature, false));
}

IN_PROC_BROWSER_TEST_F(DictationIphControllerUiTest, DoesNotTakeFocus) {
  RunTestSequence(ShowPromoOnTextarea(),
                  CheckJsResult(kTabId, "() => document.activeElement.id",
                                std::string("textarea_id")));
}

IN_PROC_BROWSER_TEST_F(DictationIphControllerUiTest,
                       ClosesAfterThreeKeyPresses) {
  RunTestSequence(
      ShowPromoOnTextarea(), SendKeyPress(kBrowserViewElementId, ui::VKEY_A),
      SendKeyPress(kBrowserViewElementId, ui::VKEY_B),
      SendKeyPress(kBrowserViewElementId, ui::VKEY_C), WaitForPromoToClose());
}

IN_PROC_BROWSER_TEST_F(DictationIphControllerUiTest,
                       ClosesWhenUserMovesToSmallField) {
  RunTestSequence(ShowPromoOnTextarea(), ClickField(kTextInput),
                  WaitForPromoToClose());
}

IN_PROC_BROWSER_TEST_F(DictationIphControllerUiTest,
                       ClosesWhenScriptMovesFocusToSmallField) {
  RunTestSequence(
      ShowPromoOnTextarea(),
      ExecuteJs(kTabId, "() => document.getElementById('text_id').focus()"),
      WaitForPromoToClose());
}

IN_PROC_BROWSER_TEST_F(DictationIphControllerUiTest, ClosesOnNavigation) {
  RunTestSequence(
      ShowPromoOnTextarea(),
      NavigateWebContents(kTabId, embedded_test_server()->GetURL(
                                      "/textinput/simple_textarea.html")),
      WaitForPromoToClose());
}

IN_PROC_BROWSER_TEST_F(DictationIphControllerUiTest, ClosesOnIframeNavigation) {
  // Navigate only the iframe, cross-site, so its new document gets a new
  // RenderFrameHost while the primary page stays the same.
  const GURL url = embedded_test_server()->GetURL("b.com", "/title1.html");
  RunTestSequence(
      ShowPromoOnTextareaInIframe(),
      ExecuteJs(kTabId,
                content::JsReplace("() => { document.getElementById('frame')"
                                   ".contentWindow.location = $1; }",
                                   url)),
      WaitForPromoToClose());
}

IN_PROC_BROWSER_TEST_F(DictationIphControllerUiTest,
                       ClosesWhenIframeIsRemoved) {
  RunTestSequence(
      ShowPromoOnTextareaInIframe(),
      ExecuteJs(kTabId, "() => document.getElementById('frame').remove()"),
      WaitForPromoToClose());
}

IN_PROC_BROWSER_TEST_F(DictationIphControllerUiTest, ClosesOnTabSwitch) {
  RunTestSequence(ShowPromoOnTextarea(),
                  AddInstrumentedTab(kSecondTabId,
                                     embedded_test_server()->GetURL(kPagePath)),
                  WaitForPromoToClose());
}

}  // namespace dictation
