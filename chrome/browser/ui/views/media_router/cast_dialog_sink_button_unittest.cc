// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label.h"

namespace media_router {

class CastDialogSinkButtonTest : public ChromeViewsTestBase {
 public:
  CastDialogSinkButtonTest() = default;

  CastDialogSinkButtonTest(const CastDialogSinkButtonTest&) = delete;
  CastDialogSinkButtonTest& operator=(const CastDialogSinkButtonTest&) = delete;

  ~CastDialogSinkButtonTest() override = default;
};

TEST_F(CastDialogSinkButtonTest, SetTitleLabel) {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.friendly_name = u"sink name";
  CastDialogSinkButton button(views::Button::PressedCallback(), sink);
  EXPECT_EQ(sink.friendly_name, button.title()->GetText());
}

TEST_F(CastDialogSinkButtonTest, SetStatusLabelForAvailableSink) {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.state = UIMediaSinkState::AVAILABLE;
  CastDialogSinkButton button(views::Button::PressedCallback(), sink);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_AVAILABLE),
            button.subtitle()->GetText());
  // Disabling an AVAILABLE sink button should change its label to "Source not
  // supported".
  button.SetEnabled(false);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SOURCE_NOT_SUPPORTED),
            button.subtitle()->GetText());
  // Re-enabling it should make set the label to "Available" again.
  button.SetEnabled(true);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_AVAILABLE),
            button.subtitle()->GetText());
}

TEST_F(CastDialogSinkButtonTest, SetStatusLabelForActiveSink) {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.state = UIMediaSinkState::CONNECTING;
  CastDialogSinkButton button1(views::Button::PressedCallback(), sink);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_CONNECTING),
            button1.subtitle()->GetText());

  sink.state = UIMediaSinkState::CONNECTED;
  sink.status_text = u"status text";
  CastDialogSinkButton button2(views::Button::PressedCallback(), sink);
  EXPECT_EQ(sink.status_text, button2.subtitle()->GetText());

  // The status label should be "Disconnecting..." even if |status_text| is set.
  sink.state = UIMediaSinkState::DISCONNECTING;
  CastDialogSinkButton button3(views::Button::PressedCallback(), sink);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_DISCONNECTING),
            button3.subtitle()->GetText());
}

TEST_F(CastDialogSinkButtonTest, SetStatusLabelForSinkWithIssue) {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.issue = Issue::CreateIssueWithIssueInfo(
      IssueInfo("issue", IssueInfo::Severity::WARNING, "sinkId1"));
  // Issue info should be the status text regardless of the sink state.
  sink.state = UIMediaSinkState::AVAILABLE;
  CastDialogSinkButton button1(views::Button::PressedCallback(), sink);
  EXPECT_EQ(base::UTF8ToUTF16(sink.issue->info().title),
            button1.subtitle()->GetText());
  sink.state = UIMediaSinkState::CONNECTED;
  CastDialogSinkButton button2(views::Button::PressedCallback(), sink);
  EXPECT_EQ(base::UTF8ToUTF16(sink.issue->info().title),
            button2.subtitle()->GetText());
}

TEST_F(CastDialogSinkButtonTest, SetStatusLabelForDialSinks) {
  UIMediaSink sink{mojom::MediaRouteProviderId::DIAL};
  sink.state = UIMediaSinkState::AVAILABLE;
  sink.cast_modes = {MediaCastMode::PRESENTATION};
  CastDialogSinkButton button1(views::Button::PressedCallback(), sink);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_AVAILABLE),
            button1.subtitle()->GetText());

  // If the sink is available (has no active session) and is incompatible with
  // the current sender page, the status text should say that the device is only
  // available on certain sites.
  sink.cast_modes = {};
  CastDialogSinkButton button2(views::Button::PressedCallback(), sink);
  button2.SetEnabled(false);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_AVAILABLE_SPECIFIC_SITES),
      button2.subtitle()->GetText());

  // If the sink is connected, we should show the session info, even if the
  // device is incompatible with the current sender page.
  sink.state = UIMediaSinkState::CONNECTED;
  sink.status_text = u"YouTube";
  CastDialogSinkButton button3(views::Button::PressedCallback(), sink);
  EXPECT_EQ(sink.status_text, button3.subtitle()->GetText());
}

TEST_F(CastDialogSinkButtonTest, OverrideStatusText) {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  CastDialogSinkButton button(views::Button::PressedCallback(), sink);
  std::u16string status0 = u"status0";
  std::u16string status1 = u"status1";
  std::u16string status2 = u"status2";

  // Calling RestoreStatusText does nothing when status has not been overridden.
  button.subtitle()->SetText(status0);
  ASSERT_EQ(button.subtitle()->GetText(), status0);
  button.RestoreStatusText();
  EXPECT_EQ(button.subtitle()->GetText(), status0);

  // OverrideStatusText replaces status text.
  button.OverrideStatusText(status1);
  EXPECT_EQ(button.subtitle()->GetText(), status1);

  // Additional calls to OverrideStatusText change the text.
  button.OverrideStatusText(status2);
  EXPECT_EQ(button.subtitle()->GetText(), status2);

  // RestoreStatusText restores the saved status text.
  button.RestoreStatusText();
  EXPECT_EQ(button.subtitle()->GetText(), status0);

  // Additional calls to RestoreStatusText don't change the text.
  button.subtitle()->SetText(status1);
  ASSERT_EQ(button.subtitle()->GetText(), status1);
  button.RestoreStatusText();
  EXPECT_EQ(button.subtitle()->GetText(), status1);
}

}  // namespace media_router
