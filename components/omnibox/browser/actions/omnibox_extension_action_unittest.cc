// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_extension_action.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

constexpr char16_t kLabel[] = u"label";
constexpr char16_t kTooltipText[] = u"tooltip text";

scoped_refptr<OmniboxExtensionAction> CreateSimpleAction(
    base::RepeatingClosure on_action_executed) {
  return base::MakeRefCounted<OmniboxExtensionAction>(
      kLabel, kTooltipText, std::move(on_action_executed), gfx::Image());
}

scoped_refptr<OmniboxExtensionAction> CreateSimpleActionWithIcon(
    base::RepeatingClosure on_action_executed) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseColor(SK_ColorRED);
  gfx::Image image = gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
  return base::MakeRefCounted<OmniboxExtensionAction>(
      kLabel, kTooltipText, std::move(on_action_executed), image);
}
}  // namespace

TEST(OmniboxExtensionActionTest, BasicInfo) {
  scoped_refptr<OmniboxExtensionAction> action =
      CreateSimpleAction(base::DoNothing());

  EXPECT_EQ(action->ActionId(), OmniboxActionId::EXTENSION_ACTION);

  // Ensure actions have the correct labels.
  const auto& labels = action->GetLabelStrings();
  EXPECT_EQ(labels.hint, kLabel);
  EXPECT_EQ(labels.suggestion_contents, kTooltipText);
  EXPECT_EQ(labels.accessibility_suffix,
            l10n_util::GetStringUTF16(
                IDS_ACC_OMNIBOX_ACTION_IN_EXTENSION_SUGGEST_SUFFIX));
  EXPECT_EQ(labels.accessibility_hint, kTooltipText);
}

TEST(OmniboxExtensionActionTest, ActionRunnerIsInvoked) {
  auto on_action_executed = base::MockRepeatingClosure();
  EXPECT_CALL(on_action_executed, Run()).Times(1);
  scoped_refptr<OmniboxExtensionAction> action =
      CreateSimpleAction(on_action_executed.Get());

  MockAutocompleteProviderClient autocomplete_provider_client;
  OmniboxAction::ExecutionContext context(
      autocomplete_provider_client,
      OmniboxAction::ExecutionContext::OpenUrlCallback(), {},
      WindowOpenDisposition::CURRENT_TAB);
  action->Execute(context);
}

TEST(OmniboxExtensionActionTest, ImageIsSet) {
  auto on_action_executed = base::MockRepeatingClosure();
  scoped_refptr<OmniboxExtensionAction> action =
      CreateSimpleActionWithIcon(on_action_executed.Get());

  gfx::Image image = action->GetIconImage();
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseColor(SK_ColorRED);
  gfx::test::CheckColors(bitmap.getColor(0, 0),
                         image.ToSkBitmap()->getColor(0, 0));
}
