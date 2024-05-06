// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/permission_toggle_row_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/view_utils.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);

}  // namespace

// Test permissions UI behavior when the flag BlockMidiByDefault is enabled.
class MidiPermissionsFlowInteractiveUITest : public InteractiveBrowserTest {
 public:
  MidiPermissionsFlowInteractiveUITest() {
    feature_list_.InitAndEnableFeature(blink::features::kBlockMidiByDefault);
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  ~MidiPermissionsFlowInteractiveUITest() override = default;
  MidiPermissionsFlowInteractiveUITest(
      const MidiPermissionsFlowInteractiveUITest&) = delete;
  void operator=(const MidiPermissionsFlowInteractiveUITest&) = delete;

  void SetUp() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    ASSERT_TRUE(https_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_server());
    https_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  ui::ElementContext context() const {
    return browser()->window()->GetElementContext();
  }

  auto NavigateAndRequestMidi() {
    return Steps(
        InstrumentTab(kWebContentsElementId),
        NavigateWebContents(kWebContentsElementId, GetURL()),
        // TODO(crbug.com/40063295) Change this call back to just
        // `navigator.requestMIDIAccess` once the feature is ready
        ExecuteJs(kWebContentsElementId,
                  "() => { navigator.requestMIDIAccess( { sysex: true } ) }"),
        WaitForShow(PermissionPromptBubbleBaseView::kMainViewId));
  }

 protected:
  GURL GetURL() {
    return https_server()->GetURL("a.test", "/permissions/requests.html");
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Display MIDI permission prompt.
IN_PROC_BROWSER_TEST_F(MidiPermissionsFlowInteractiveUITest, PermissionPrompt) {
  RunTestSequenceInContext(
      context(), NavigateAndRequestMidi(),
      CheckViewProperty(
          PermissionPromptBubbleBaseView::kMainViewId,
          &PermissionPromptBubbleBaseView::GetPermissionFragmentForTesting,
          l10n_util::GetStringFUTF16(
              IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_ONE_PERM, u"",
              l10n_util::GetStringUTF16(IDS_MIDI_SYSEX_PERMISSION_FRAGMENT))));
}

// Display MIDI permission state in page info when denied.
IN_PROC_BROWSER_TEST_F(MidiPermissionsFlowInteractiveUITest,
                       BlockedMidiPermissionInPageInfo) {
  RunTestSequenceInContext(
      context(), NavigateAndRequestMidi(),
      PressButton(PermissionPromptBubbleBaseView::kBlockButtonElementId),
      WaitForHide(PermissionPromptBubbleBaseView::kMainViewId),
      PressButton(kLocationIconElementId),  // open page info.
      AfterShow(
          PageInfoMainView::kMainLayoutElementId,
          base::BindLambdaForTesting([](ui::TrackedElement* element) {
            bool includes_midi_sysex = false;
            for (PermissionToggleRowView* permission_toggle_row :
                 AsView<PageInfoMainView>(element)->GetToggleRowsForTesting()) {
              if (permission_toggle_row->GetRowTitleForTesting() ==
                         l10n_util::GetStringUTF16(
                             IDS_SITE_SETTINGS_TYPE_MIDI_SYSEX)) {
                includes_midi_sysex = true;
              }
            }
            EXPECT_TRUE(includes_midi_sysex);
          })));
}

// Display MIDI permission state in page info when allowed.
IN_PROC_BROWSER_TEST_F(MidiPermissionsFlowInteractiveUITest,
                       AllowedMidiPermissionInPageInfo) {
  RunTestSequenceInContext(
      context(), NavigateAndRequestMidi(),
      PressButton(PermissionPromptBubbleBaseView::kAllowButtonElementId),
      WaitForHide(PermissionPromptBubbleBaseView::kMainViewId),
      PressButton(kLocationIconElementId),  // open page info.
      AfterShow(
          PageInfoMainView::kMainLayoutElementId,
          base::BindLambdaForTesting([](ui::TrackedElement* element) {
            bool includes_midi_sysex = false;
            for (PermissionToggleRowView* permission_toggle_row :
                 AsView<PageInfoMainView>(element)->GetToggleRowsForTesting()) {
              if (permission_toggle_row->GetRowTitleForTesting() ==
                         l10n_util::GetStringUTF16(
                             IDS_SITE_SETTINGS_TYPE_MIDI_SYSEX)) {
                includes_midi_sysex = true;
              }
            }
            EXPECT_TRUE(includes_midi_sysex);
          })));
}

// Display blockage indicator of MIDI when blocked.
IN_PROC_BROWSER_TEST_F(MidiPermissionsFlowInteractiveUITest,
                       BlockedMidiPermissionIndicator) {
  RunTestSequenceInContext(
      context(), NavigateAndRequestMidi(),
      PressButton(PermissionPromptBubbleBaseView::kBlockButtonElementId),
      WaitForHide(PermissionPromptBubbleBaseView::kMainViewId),
      AfterShow(
          ContentSettingImageView::kMidiSysexActivityIndicatorElementId,
          base::BindOnce([](ui::TrackedElement* element) {
            auto* element_view = AsView<ContentSettingImageView>(element);
            EXPECT_EQ(element_view->get_icon_for_testing(),
                      &vector_icons::kMidiOffChromeRefreshIcon);
            EXPECT_EQ(element_view->get_icon_badge_for_testing(),
                      &gfx::kNoneIcon);
            EXPECT_EQ(
                element_view->get_tooltip_text_for_testing(),
                l10n_util::GetStringUTF16(IDS_BLOCKED_MIDI_SYSEX_MESSAGE));
          })));
}

// Display in-use indicator of MIDI when allowed.
IN_PROC_BROWSER_TEST_F(MidiPermissionsFlowInteractiveUITest,
                       AllowedMidiPermissionIndicator) {
  RunTestSequenceInContext(
      context(), NavigateAndRequestMidi(),
      PressButton(PermissionPromptBubbleBaseView::kAllowButtonElementId),
      WaitForHide(PermissionPromptBubbleBaseView::kMainViewId),
      AfterShow(
          ContentSettingImageView::kMidiSysexActivityIndicatorElementId,
          base::BindOnce([](ui::TrackedElement* element) {
            auto* element_view = AsView<ContentSettingImageView>(element);
            EXPECT_EQ(element_view->get_icon_for_testing(),
                      &vector_icons::kMidiChromeRefreshIcon);
            EXPECT_EQ(element_view->get_icon_badge_for_testing(),
                      &gfx::kNoneIcon);
            EXPECT_EQ(
                element_view->get_tooltip_text_for_testing(),
                l10n_util::GetStringUTF16(IDS_ALLOWED_MIDI_SYSEX_MESSAGE));
          })));
}
