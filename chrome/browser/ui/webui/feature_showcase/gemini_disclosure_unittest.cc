// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/gemini_disclosure.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Field;

TEST(GeminiDisclosureTest, UsRegion) {
  const GeminiDisclosure disclosure =
      GetGeminiDisclosure(/*country_code=*/"us", /*is_managed=*/false);
  EXPECT_THAT(
      disclosure,
      AllOf(
          Field(&GeminiDisclosure::first_paragraph,
                u"Chats are reviewed and used to improve Google AI. Gemini is "
                u"AI and can make mistakes. Some content may not be suitable "
                u"for everyone. You can <a "
                u"href=\"https://myactivity.google.com/product/gemini\" "
                u"target=\"_blank\">manage your activity</a>, including info "
                u"about your location. <a "
                u"href=\"https://support.google.com/gemini/"
                u"?p=activity_settings\" target=\"_blank\">Learn more about "
                u"your choices</a>"),
          Field(&GeminiDisclosure::second_paragraph,
                u"<a href=\"https://policies.google.com/terms\" "
                u"target=\"_blank\">Google Terms</a> and the <a "
                u"href=\"https://support.google.com/gemini/answer/13594961\" "
                u"target=\"_blank\">Gemini Apps Privacy Notice</a> apply")));
}

TEST(GeminiDisclosureTest, UsRegionManaged) {
  const GeminiDisclosure disclosure =
      GetGeminiDisclosure(/*country_code=*/"us", /*is_managed=*/true);
  EXPECT_THAT(
      disclosure,
      AllOf(
          Field(
              &GeminiDisclosure::first_paragraph,
              u"Neither your conversations with Gemini nor your <a "
              u"href=\"https://knowledge.workspace.google.com/admin/"
              u"generative-ai/generative-ai-in-google-workspace-privacy-hub\" "
              u"target=\"_blank\">Google Workspace data</a> will be reviewed "
              u"or used to improve generative AI models. Gemini is AI and "
              u"can make mistakes. Some content may not be suitable for "
              u"everyone."),
          Field(&GeminiDisclosure::second_paragraph,
                u"<a href=\"https://policies.google.com/terms\" "
                u"target=\"_blank\">Google Terms</a> and the <a "
                u"href=\"https://support.google.com/gemini/answer/13594961\" "
                u"target=\"_blank\">Gemini Apps Privacy Notice</a> apply")));
}

TEST(GeminiDisclosureTest, KrRegion) {
  const GeminiDisclosure disclosure =
      GetGeminiDisclosure(/*country_code=*/"kr", /*is_managed=*/false);
  EXPECT_THAT(
      disclosure,
      AllOf(
          Field(&GeminiDisclosure::first_paragraph,
                u"Chats are reviewed and used to improve Google AI. Gemini is "
                u"AI and can make mistakes. You can <a "
                u"href=\"https://myactivity.google.com/product/gemini\" "
                u"target=\"_blank\">manage your activity</a>, including info "
                u"about your location. <a "
                u"href=\"https://support.google.com/gemini/"
                u"?p=activity_settings\" target=\"_blank\">Learn more about "
                u"your choices</a>"),
          Field(
              &GeminiDisclosure::second_paragraph,
              u"Our <a href=\"https://policies.google.com/terms\" "
              u"target=\"_blank\">Terms</a>, <a "
              u"href=\"https://www.google.com/intl/ko/policies/terms/"
              u"location\" target=\"_blank\">Korean Location Terms</a>, and "
              u"<a href=\"https://support.google.com/gemini/answer/13594961\" "
              u"target=\"_blank\">Privacy Notice</a> apply.")));
}

TEST(GeminiDisclosureTest, KrRegionManaged) {
  const GeminiDisclosure disclosure =
      GetGeminiDisclosure(/*country_code=*/"kr", /*is_managed=*/true);
  EXPECT_THAT(
      disclosure,
      AllOf(
          Field(
              &GeminiDisclosure::first_paragraph,
              u"Neither your conversations with Gemini nor your <a "
              u"href=\"https://knowledge.workspace.google.com/admin/"
              u"generative-ai/generative-ai-in-google-workspace-privacy-hub\" "
              u"target=\"_blank\">Google Workspace data</a> will be reviewed "
              u"or used to improve generative AI models. Gemini is AI and "
              u"can make mistakes."),
          Field(
              &GeminiDisclosure::second_paragraph,
              u"Our <a href=\"https://policies.google.com/terms\" "
              u"target=\"_blank\">Terms</a>, <a "
              u"href=\"https://www.google.com/intl/ko/policies/terms/"
              u"location\" target=\"_blank\">Korean Location Terms</a>, and "
              u"<a href=\"https://support.google.com/gemini/answer/13594961\" "
              u"target=\"_blank\">Privacy Notice</a> apply.")));
}

TEST(GeminiDisclosureTest, RowRegion) {
  const GeminiDisclosure disclosure =
      GetGeminiDisclosure(/*country_code=*/"gb", /*is_managed=*/false);
  EXPECT_THAT(
      disclosure,
      AllOf(
          Field(&GeminiDisclosure::first_paragraph,
                u"Chats are reviewed and used to improve Google AI. Gemini is "
                u"AI and can make mistakes. You can <a "
                u"href=\"https://myactivity.google.com/product/gemini\" "
                u"target=\"_blank\">manage your activity</a>, including info "
                u"about your location. <a "
                u"href=\"https://support.google.com/gemini/"
                u"?p=activity_settings\" target=\"_blank\">Learn more about "
                u"your choices</a>"),
          Field(&GeminiDisclosure::second_paragraph,
                u"<a href=\"https://policies.google.com/terms\" "
                u"target=\"_blank\">Google Terms</a> and the <a "
                u"href=\"https://support.google.com/gemini/answer/13594961\" "
                u"target=\"_blank\">Gemini Apps Privacy Notice</a> apply")));
}

TEST(GeminiDisclosureTest, RowRegionManaged) {
  const GeminiDisclosure disclosure =
      GetGeminiDisclosure(/*country_code=*/"gb", /*is_managed=*/true);
  EXPECT_THAT(
      disclosure,
      AllOf(
          Field(
              &GeminiDisclosure::first_paragraph,
              u"Neither your conversations with Gemini nor your <a "
              u"href=\"https://knowledge.workspace.google.com/admin/"
              u"generative-ai/generative-ai-in-google-workspace-privacy-hub\" "
              u"target=\"_blank\">Google Workspace data</a> will be reviewed "
              u"or used to improve generative AI models. Gemini is AI and "
              u"can make mistakes."),
          Field(&GeminiDisclosure::second_paragraph,
                u"<a href=\"https://policies.google.com/terms\" "
                u"target=\"_blank\">Google Terms</a> and the <a "
                u"href=\"https://support.google.com/gemini/answer/13594961\" "
                u"target=\"_blank\">Gemini Apps Privacy Notice</a> apply")));
}
