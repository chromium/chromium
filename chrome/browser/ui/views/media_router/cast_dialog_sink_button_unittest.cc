// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label.h"

namespace media_router {

class CastDialogSinkButtonTest : public ChromeViewsTestBase {
 public:
  CastDialogSinkButtonTest() = default;
  ~CastDialogSinkButtonTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastDialogSinkButtonTest);
};

TEST_F(CastDialogSinkButtonTest, SetTitleLabel) {
  UIMediaSink sink;
  sink.friendly_name = base::UTF8ToUTF16("sink name");
  CastDialogSinkButton button(nullptr, sink, 0);
  EXPECT_EQ(sink.friendly_name, button.title()->GetText());
}

TEST_F(CastDialogSinkButtonTest, SetStatusLabelForAvailableSink) {
  UIMediaSink sink;
  sink.state = UIMediaSinkState::AVAILABLE;
  CastDialogSinkButton button(nullptr, sink, 0);
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
  UIMediaSink sink;
  sink.state = UIMediaSinkState::CONNECTING;
  CastDialogSinkButton button1(nullptr, sink, 0);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_CONNECTING),
            button1.subtitle()->GetText());

  sink.state = UIMediaSinkState::CONNECTED;
  sink.status_text = base::UTF8ToUTF16("status text");
  CastDialogSinkButton button2(nullptr, sink, 1);
  EXPECT_EQ(sink.status_text, button2.subtitle()->GetText());

  // The status label should be "Disconnecting..." even if |status_text| is set.
  sink.state = UIMediaSinkState::DISCONNECTING;
  CastDialogSinkButton button3(nullptr, sink, 2);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_DISCONNECTING),
            button3.subtitle()->GetText());
}

TEST_F(CastDialogSinkButtonTest, SetStatusLabelForSinkWithIssue) {
  UIMediaSink sink;
  sink.issue = Issue(IssueInfo("issue", IssueInfo::Action::DISMISS,
                               IssueInfo::Severity::WARNING));
  // Issue info should be the status text regardless of the sink state.
  sink.state = UIMediaSinkState::AVAILABLE;
  CastDialogSinkButton button1(nullptr, sink, 0);
  EXPECT_EQ(base::UTF8ToUTF16(sink.issue->info().title),
            button1.subtitle()->GetText());
  sink.state = UIMediaSinkState::CONNECTED;
  CastDialogSinkButton button2(nullptr, sink, 1);
  EXPECT_EQ(base::UTF8ToUTF16(sink.issue->info().title),
            button2.subtitle()->GetText());
}

TEST_F(CastDialogSinkButtonTest, OverrideStatusText) {
  UIMediaSink sink;
  CastDialogSinkButton button(nullptr, sink, 0);
  base::string16 status0 = base::ASCIIToUTF16("status0");
  base::string16 status1 = base::ASCIIToUTF16("status1");
  base::string16 status2 = base::ASCIIToUTF16("status2");

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
