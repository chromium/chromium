// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/server/notes_repository.h"

#include "base/test/scoped_feature_list.h"
#include "components/content_creation/notes/core/note_features.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_creation {

class NotesRepositoryTest : public testing::Test {
 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

// Tests that IsPublishAvailable is true when the channel is Canary and the
// appropriate feature flag is set.
TEST_F(NotesRepositoryTest, IsPublishAvailable_Available) {
  scoped_feature_list_.InitAndEnableFeature(kWebNotesPublish);

  NotesRepository notes_repository(/*identity_manager=*/nullptr,
                                   url_loader_factory_,
                                   version_info::Channel::CANARY);

  ASSERT_TRUE(notes_repository.IsPublishAvailable());
}

// Tests that IsPublishAvailable returns false when the channel is not Canary,
// even if the feature flag is enabled.
TEST_F(NotesRepositoryTest, IsPublishAvailable_NotCanary) {
  scoped_feature_list_.InitAndEnableFeature(kWebNotesPublish);

  // Unknown Channel.
  NotesRepository notes_repository_unknown(/*identity_manager=*/nullptr,
                                           url_loader_factory_,
                                           version_info::Channel::UNKNOWN);
  EXPECT_FALSE(notes_repository_unknown.IsPublishAvailable());

  // Dev Channel.
  NotesRepository notes_repository_dev(/*identity_manager=*/nullptr,
                                       url_loader_factory_,
                                       version_info::Channel::DEV);
  EXPECT_FALSE(notes_repository_dev.IsPublishAvailable());

  // Beta Channel.
  NotesRepository notes_repository_beta(/*identity_manager=*/nullptr,
                                        url_loader_factory_,
                                        version_info::Channel::BETA);
  EXPECT_FALSE(notes_repository_beta.IsPublishAvailable());

  // Stable Channel
  NotesRepository notes_repository_stable(/*identity_manager=*/nullptr,
                                          url_loader_factory_,
                                          version_info::Channel::STABLE);
  EXPECT_FALSE(notes_repository_stable.IsPublishAvailable());
}

// Tests that IsPublishAvailable returns false when the feature flag is not
// enabled, even if the channel is Canary.
TEST_F(NotesRepositoryTest, IsPublishAvailable_FeatureDisabled) {
  NotesRepository notes_repository(/*identity_manager=*/nullptr,
                                   url_loader_factory_,
                                   version_info::Channel::CANARY);

  ASSERT_FALSE(notes_repository.IsPublishAvailable());
}

}  // namespace content_creation
