// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/gemini_disclosure.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Field;

TEST(GeminiDisclosureTest, USRegion) {
  GeminiDisclosure disclosure =
      GetGeminiDisclosure(/*country_code=*/"us", /*is_managed=*/false);
  EXPECT_THAT(
      disclosure,
      AllOf(Field(&GeminiDisclosure::first_paragraph,
                  u"Google uses the content and URL of shared tabs. Your "
                  u"current tab is "
                  u"shared in new chats. You can change this setting anytime."),
            Field(&GeminiDisclosure::second_paragraph,
                  u"Chats are reviewed and used to improve Google AI. Gemini "
                  u"is AI and can "
                  u"make mistakes. Some content may not be suitable for "
                  u"everyone. You can "
                  u"<a href=\"https://myactivity.google.com/product/gemini\" "
                  u"target=\"_blank\">manage your activity</a>, including info "
                  u"about your "
                  u"location. <a "
                  u"href=\"https://support.google.com/gemini/"
                  u"?p=activity_settings\" "
                  u"target=\"_blank\">Learn more about your choices</a>"),
            Field(&GeminiDisclosure::third_paragraph,
                  u"<a href=\"https://policies.google.com/terms\" "
                  u"target=\"_blank\">Google Terms</a> and the <a "
                  u"href=\"https://support.google.com/gemini/answer/13594961\" "
                  u"target=\"_blank\">Gemini Apps Privacy Notice</a> apply")));
}

TEST(GeminiDisclosureTest, USRegionManaged) {
  GeminiDisclosure disclosure =
      GetGeminiDisclosure(/*country_code=*/"us", /*is_managed=*/true);
  EXPECT_THAT(
      disclosure,
      AllOf(Field(&GeminiDisclosure::first_paragraph,
                  u"Google uses the content and URL of shared tabs. Your "
                  u"current tab is "
                  u"shared in new chats. You can change this setting anytime."),
            Field(&GeminiDisclosure::second_paragraph,
                  u"Neither your conversations with Gemini nor your Google "
                  u"Workspace data "
                  u"will be reviewed or used to improve generative AI models. "
                  u"Gemini is AI "
                  u"and can make mistakes. Some content may not be suitable "
                  u"for everyone."),
            Field(&GeminiDisclosure::third_paragraph,
                  u"<a href=\"https://policies.google.com/terms\" "
                  u"target=\"_blank\">Google Terms</a> and the <a "
                  u"href=\"https://support.google.com/gemini/answer/13594961\" "
                  u"target=\"_blank\">Gemini Apps Privacy Notice</a> apply")));
}

TEST(GeminiDisclosureTest, KRRegion) {
  GeminiDisclosure disclosure =
      GetGeminiDisclosure(/*country_code=*/"kr", /*is_managed=*/false);
  EXPECT_THAT(
      disclosure,
      AllOf(
          Field(&GeminiDisclosure::first_paragraph,
                u"Google uses the content and URL of shared tabs. Your current "
                u"tab is "
                u"shared in new chats. You can change this setting anytime."),
          Field(&GeminiDisclosure::second_paragraph,
                u"Chats are reviewed and used to improve Google AI. Gemini is "
                u"AI and can "
                u"make mistakes. You can <a "
                u"href=\"https://myactivity.google.com/product/gemini\" "
                u"target=\"_blank\">manage your activity</a>, including info "
                u"about your "
                u"location. <a "
                u"href=\"https://support.google.com/gemini/"
                u"?p=activity_settings\" "
                u"target=\"_blank\">Learn more about your choices</a>"),
          Field(&GeminiDisclosure::third_paragraph,
                u"Our <a href=\"https://policies.google.com/terms\" "
                u"target=\"_blank\">Terms</a>, <a "
                u"href=\"https://www.google.com/intl/ko/policies/terms/"
                u"location\" "
                u"target=\"_blank\">Korean Location Terms</a>, and <a "
                u"href=\"https://support.google.com/gemini?p=privacy_notice\" "
                u"target=\"_blank\">Privacy Notice</a> apply.")));
}

TEST(GeminiDisclosureTest, KRRegionManaged) {
  GeminiDisclosure disclosure =
      GetGeminiDisclosure(/*country_code=*/"kr", /*is_managed=*/true);
  EXPECT_THAT(
      disclosure,
      AllOf(
          Field(&GeminiDisclosure::first_paragraph,
                u"Google uses the content and URL of shared tabs. Your current "
                u"tab is "
                u"shared in new chats. You can change this setting anytime."),
          Field(&GeminiDisclosure::second_paragraph,
                u"Neither your conversations with Gemini nor your Google "
                u"Workspace data "
                u"will be reviewed or used to improve generative AI models. "
                u"Gemini is AI "
                u"and can make mistakes."),
          Field(&GeminiDisclosure::third_paragraph,
                u"Our <a href=\"https://policies.google.com/terms\" "
                u"target=\"_blank\">Terms</a>, <a "
                u"href=\"https://www.google.com/intl/ko/policies/terms/"
                u"location\" "
                u"target=\"_blank\">Korean Location Terms</a>, and <a "
                u"href=\"https://support.google.com/gemini?p=privacy_notice\" "
                u"target=\"_blank\">Privacy Notice</a> apply.")));
}

TEST(GeminiDisclosureTest, RowRegion) {
  GeminiDisclosure disclosure =
      GetGeminiDisclosure(/*country_code=*/"gb", /*is_managed=*/false);
  EXPECT_THAT(
      disclosure,
      AllOf(Field(&GeminiDisclosure::first_paragraph,
                  u"Google uses the content and URL of shared tabs. Your "
                  u"current tab is "
                  u"shared in new chats. You can change this setting anytime."),
            Field(&GeminiDisclosure::second_paragraph,
                  u"Chats are reviewed and used to improve Google AI. Gemini "
                  u"is AI and can "
                  u"make mistakes. You can <a "
                  u"href=\"https://myactivity.google.com/product/gemini\" "
                  u"target=\"_blank\">manage your activity</a>, including info "
                  u"about your "
                  u"location. <a "
                  u"href=\"https://support.google.com/gemini/"
                  u"?p=activity_settings\" "
                  u"target=\"_blank\">Learn more about your choices</a>"),
            Field(&GeminiDisclosure::third_paragraph,
                  u"<a href=\"https://policies.google.com/terms\" "
                  u"target=\"_blank\">Google Terms</a> and the <a "
                  u"href=\"https://support.google.com/gemini/answer/13594961\" "
                  u"target=\"_blank\">Gemini Apps Privacy Notice</a> apply")));
}

TEST(GeminiDisclosureTest, RowRegionManaged) {
  GeminiDisclosure disclosure =
      GetGeminiDisclosure(/*country_code=*/"gb", /*is_managed=*/true);
  EXPECT_THAT(
      disclosure,
      AllOf(Field(&GeminiDisclosure::first_paragraph,
                  u"Google uses the content and URL of shared tabs. Your "
                  u"current tab is "
                  u"shared in new chats. You can change this setting anytime."),
            Field(&GeminiDisclosure::second_paragraph,
                  u"Neither your conversations with Gemini nor your Google "
                  u"Workspace data "
                  u"will be reviewed or used to improve generative AI models. "
                  u"Gemini is AI "
                  u"and can make mistakes."),
            Field(&GeminiDisclosure::third_paragraph,
                  u"<a href=\"https://policies.google.com/terms\" "
                  u"target=\"_blank\">Google Terms</a> and the <a "
                  u"href=\"https://support.google.com/gemini/answer/13594961\" "
                  u"target=\"_blank\">Gemini Apps Privacy Notice</a> apply")));
}
