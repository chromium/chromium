// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/feed_user_segment.h"

#include <array>

#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {

// List of sub-segments for Feed segment.
enum class FeedUserSubsegment {
  kUnknown = 0,
  kOther = 1,

  // Legacy groups, split into feed engagement types below.
  kDeprecatedActiveOnFeedOnly = 2,
  kDeprecatedActiveOnFeedAndNtpFeatures = 3,

  // Recorded when no feed usage was observed.
  kNoFeedAndNtpFeatures = 4,
  kMvtOnly = 5,
  kReturnToCurrentTabOnly = 6,
  kUsedNtpWithoutModules = 7,
  kNoNTPOrHomeOpened = 8,

  // Cut-off after which the model returns Feed user as final segment.

  // Feed engagement combined with NTP features.
  kNtpAndFeedEngaged = 9,
  kNtpAndFeedEngagedSimple = 10,
  kNtpAndFeedScrolled = 11,
  kNtpAndFeedInteracted = 12,
  kNoNtpAndFeedEngaged = 13,
  kNoNtpAndFeedEngagedSimple = 14,
  kNoNtpAndFeedScrolled = 15,
  kNoNtpAndFeedInteracted = 16,
  kMaxValue = kNoNtpAndFeedInteracted
};

#define RANK(x) static_cast<int>(x)

using proto::SegmentId;

// Default parameters for Chrome Start model.
constexpr SegmentId kFeedUserSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER;
constexpr int64_t kFeedUserSignalStorageLength = 28;
constexpr int64_t kFeedUserMinSignalCollectionLength = 7;

constexpr int kFeedUserSegmentSelectionTTLDays = 14;
constexpr int kFeedUserSegmentUnknownSelectionTTLDays = 14;

// InputFeatures.

// The enum values are based on feed::FeedEngagementType.
constexpr std::array<int32_t, 1> kFeedEngagementEngaged{0};
constexpr std::array<int32_t, 1> kFeedEngagementSimple{1};
constexpr std::array<int32_t, 1> kFeedEngagementInteracted{2};
constexpr std::array<int32_t, 1> kFeedEngagementScrolled{4};

constexpr std::array<MetadataWriter::UMAFeature, 11> kFeedUserUMAFeatures = {
    MetadataWriter::UMAFeature::FromUserAction("MobileNTPMostVisited", 14),
    MetadataWriter::UMAFeature::FromUserAction("MobileNewTabOpened", 14),
    MetadataWriter::UMAFeature::FromUserAction("MobileNewTabShown", 14),
    MetadataWriter::UMAFeature::FromUserAction("Home", 14),
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuRecentTabs", 14),
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuHistory", 14),
    MetadataWriter::UMAFeature::FromUserAction("MobileTabReturnedToCurrentTab",
                                               14),
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "ContentSuggestions.Feed.EngagementType",
        28,
        kFeedEngagementEngaged.data(),
        kFeedEngagementEngaged.size()),
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "ContentSuggestions.Feed.EngagementType",
        28,
        kFeedEngagementSimple.data(),
        kFeedEngagementSimple.size()),
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "ContentSuggestions.Feed.EngagementType",
        28,
        kFeedEngagementInteracted.data(),
        kFeedEngagementInteracted.size()),
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "ContentSuggestions.Feed.EngagementType",
        28,
        kFeedEngagementScrolled.data(),
        kFeedEngagementScrolled.size()),
};

// Any updates to these strings need to also update the field trials allowlist
// in go/segmentation-field-trials-map.
std::string FeedUserSubsegmentToString(FeedUserSubsegment feed_group) {
  switch (feed_group) {
    case FeedUserSubsegment::kUnknown:
      return "Unknown";
    case FeedUserSubsegment::kOther:
      return "Other";
    case FeedUserSubsegment::kDeprecatedActiveOnFeedOnly:
      return "ActiveOnFeedOnly";
    case FeedUserSubsegment::kDeprecatedActiveOnFeedAndNtpFeatures:
      return "ActiveOnFeedAndNtpFeatures";
    case FeedUserSubsegment::kNoFeedAndNtpFeatures:
      return "NoFeedAndNtpFeatures";
    case FeedUserSubsegment::kMvtOnly:
      return "MvtOnly";
    case FeedUserSubsegment::kReturnToCurrentTabOnly:
      return "ReturnToCurrentTabOnly";
    case FeedUserSubsegment::kUsedNtpWithoutModules:
      return "UsedNtpWithoutModules";
    case FeedUserSubsegment::kNoNTPOrHomeOpened:
      return "NoNTPOrHomeOpened";
    case FeedUserSubsegment::kNtpAndFeedEngaged:
      return "NtpAndFeedEngaged";
    case FeedUserSubsegment::kNtpAndFeedEngagedSimple:
      return "NtpAndFeedEngagedSimple";
    case FeedUserSubsegment::kNtpAndFeedScrolled:
      return "NtpAndFeedScrolled";
    case FeedUserSubsegment::kNtpAndFeedInteracted:
      return "NtpAndFeedInteracted";
    case FeedUserSubsegment::kNoNtpAndFeedEngaged:
      return "NoNtpAndFeedEngaged";
    case FeedUserSubsegment::kNoNtpAndFeedEngagedSimple:
      return "NoNtpAndFeedEngagedSimple";
    case FeedUserSubsegment::kNoNtpAndFeedScrolled:
      return "NoNtpAndFeedScrolled";
    case FeedUserSubsegment::kNoNtpAndFeedInteracted:
      return "NoNtpAndFeedInteracted";
  }
}

std::unique_ptr<ModelProvider> GetFeedUserSegmentDefautlModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          features::kSegmentationPlatformFeedSegmentFeature,
          kDefaultModelEnabledParam, true)) {
    return nullptr;
  }
  return std::make_unique<FeedUserSegment>();
}

}  // namespace

// static
std::unique_ptr<Config> FeedUserSegment::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformFeedSegmentFeature)) {
    return nullptr;
  }

  auto config = std::make_unique<Config>();
  config->segmentation_key = kFeedUserSegmentationKey;
  config->segmentation_uma_name = kFeedUserSegmentUmaName;
  config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER,
                       GetFeedUserSegmentDefautlModel());
  config->segment_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kSegmentationPlatformFeedSegmentFeature,
          kVariationsParamNameSegmentSelectionTTLDays,
          kFeedUserSegmentSelectionTTLDays));
  config->unknown_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kSegmentationPlatformFeedSegmentFeature,
          kVariationsParamNameUnknownSelectionTTLDays,
          kFeedUserSegmentUnknownSelectionTTLDays));
  config->is_boolean_segment = true;
  return config;
}

FeedUserSegment::FeedUserSegment() : ModelProvider(kFeedUserSegmentId) {}

absl::optional<std::string> FeedUserSegment::GetSubsegmentName(
    int subsegment_rank) {
  DCHECK(RANK(FeedUserSubsegment::kUnknown) <= subsegment_rank &&
         subsegment_rank <= RANK(FeedUserSubsegment::kMaxValue));
  FeedUserSubsegment subgroup =
      static_cast<FeedUserSubsegment>(subsegment_rank);
  return FeedUserSubsegmentToString(subgroup);
}

void FeedUserSegment::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  proto::SegmentationModelMetadata chrome_start_metadata;
  MetadataWriter writer(&chrome_start_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kFeedUserMinSignalCollectionLength, kFeedUserSignalStorageLength);

  // All values greater than or equal to kNtpAndFeedEngaged will map to true.
  writer.AddBooleanSegmentDiscreteMappingWithSubsegments(
      kFeedUserSegmentationKey, RANK(FeedUserSubsegment::kNtpAndFeedEngaged),
      RANK(FeedUserSubsegment::kMaxValue));

  // Set features.
  writer.AddUmaFeatures(kFeedUserUMAFeatures.data(),
                        kFeedUserUMAFeatures.size());

  constexpr int kModelVersion = 1;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindRepeating(model_updated_callback, kFeedUserSegmentId,
                          std::move(chrome_start_metadata), kModelVersion));
}

void FeedUserSegment::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kFeedUserUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  FeedUserSubsegment segment = FeedUserSubsegment::kNoNTPOrHomeOpened;

  const bool feed_engaged = inputs[7] >= 2;
  const bool feed_engaged_simple = inputs[8] >= 2;
  const bool feed_interacted = inputs[9] >= 2;
  const bool feed_scrolled = inputs[10] >= 2;

  const bool mv_tiles_used = inputs[0] >= 2;
  const bool return_to_tab_used = inputs[6] >= 2;
  const bool ntp_used = mv_tiles_used || return_to_tab_used;
  const bool home_or_ntp_opened = (inputs[1] + inputs[2] + inputs[3]) >= 2;

  if (feed_engaged) {
    segment = ntp_used ? FeedUserSubsegment::kNtpAndFeedEngaged
                       : FeedUserSubsegment::kNoNtpAndFeedEngaged;
  } else if (feed_engaged_simple) {
    segment = ntp_used ? FeedUserSubsegment::kNtpAndFeedEngagedSimple
                       : FeedUserSubsegment::kNoNtpAndFeedEngagedSimple;
  } else if (feed_interacted) {
    segment = ntp_used ? FeedUserSubsegment::kNtpAndFeedInteracted
                       : FeedUserSubsegment::kNoNtpAndFeedInteracted;
  } else if (feed_scrolled) {
    segment = ntp_used ? FeedUserSubsegment::kNtpAndFeedScrolled
                       : FeedUserSubsegment::kNoNtpAndFeedScrolled;
  } else if (home_or_ntp_opened) {
    if (mv_tiles_used && return_to_tab_used) {
      segment = FeedUserSubsegment::kNoFeedAndNtpFeatures;
    } else if (mv_tiles_used) {
      segment = FeedUserSubsegment::kMvtOnly;
    } else if (return_to_tab_used) {
      segment = FeedUserSubsegment::kReturnToCurrentTabOnly;
    } else {
      segment = segment = FeedUserSubsegment::kUsedNtpWithoutModules;
    }
  } else {
    segment = segment = FeedUserSubsegment::kNoNTPOrHomeOpened;
  }

  float result = RANK(segment);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

bool FeedUserSegment::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
