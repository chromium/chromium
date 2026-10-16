// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/plural_string_provider.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/grit/generated_resources.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

class PluralStringProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    PluralStringProvider::Create(remote_.BindNewPipeAndPassReceiver(),
                                 GetSearchboxPluralStringIds());
  }

  std::string GetPluralString(const std::string& message_name, int count) {
    base::test::TestFuture<const std::string&> future;
    remote_->GetPluralString(message_name, count, future.GetCallback());
    return future.Get();
  }

  base::test::TaskEnvironment task_environment_;
  mojo::Remote<plural_string::mojom::PluralStringProvider> remote_;
};

TEST_F(PluralStringProviderTest, SharingTabs) {
  for (int count : {1, 2, 5}) {
    EXPECT_EQ(l10n_util::GetPluralStringFUTF8(IDS_COMPOSE_SHARING_TABS, count),
              GetPluralString("sharingTabs", count));
  }
  EXPECT_NE(GetPluralString("sharingTabs", 1),
            GetPluralString("sharingTabs", 2));
}

TEST_F(PluralStringProviderTest, ServesOnlyGivenMessages) {
  mojo::Remote<plural_string::mojom::PluralStringProvider> remote;
  PluralStringProvider::Create(remote.BindNewPipeAndPassReceiver(),
                               {{"tabs", IDS_SAVED_TAB_GROUP_TABS_COUNT}});
  base::test::TestFuture<const std::string&> future;
  remote->GetPluralString("tabs", 3, future.GetCallback());
  EXPECT_EQ(l10n_util::GetPluralStringFUTF8(IDS_SAVED_TAB_GROUP_TABS_COUNT, 3),
            future.Get());

  mojo::test::BadMessageObserver bad_message_observer;
  remote->GetPluralString("sharingTabs", 1, base::DoNothing());
  EXPECT_EQ("Unknown plural string message name",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(PluralStringProviderTest, UnknownMessageIsBadMessage) {
  mojo::test::BadMessageObserver bad_message_observer;
  remote_->GetPluralString("notARealMessage", 1, base::DoNothing());
  EXPECT_EQ("Unknown plural string message name",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(PluralStringProviderTest, NegativeCountIsBadMessage) {
  mojo::test::BadMessageObserver bad_message_observer;
  remote_->GetPluralString("sharingTabs", -1, base::DoNothing());
  EXPECT_EQ("Negative plural string count",
            bad_message_observer.WaitForBadMessage());
}
