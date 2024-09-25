// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MODEL_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MODEL_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/features.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/growth/action_performer.h"

namespace base {
class Time;
class Version;
}  // namespace base

namespace gfx {
class Image;
struct VectorIcon;
}  // namespace gfx

namespace ui {
class ImageModel;
}  // namespace ui

namespace growth {

// Entries should not be renumbered and numeric values should never be reused
// as it is used for logging metrics as well. Please keep in sync with
// "CampaignSlot" in tools/metrics/histograms/metadata/ash_growth/enums.xml.
enum class Slot {
  kDemoModeApp = 0,
  kDemoModeFreePlayApps = 1,
  kNudge = 2,
  kNotification = 3,
  kOobePerkDiscovery = 4,
  kMaxValue = kOobePerkDiscovery
};

// These values are deserialized from Growth Campaign, so entries should not
// be renumbered and numeric values should never be reused.
enum class BuiltInVectorIcon { kRedeem = 0, kMaxValue = kRedeem };

// These values are deserialized from Growth Campaign, so entries should not
// be renumbered and numeric values should never be reused.
enum class BuiltInImage {
  kContainerApp = 0,
  kG1 = 1,
  kSparkRebuy = 2,
  kSpark1PApp = 3,
  kSparkV2 = 4,
  kG1Notification = 5,
  kMall = 6,
  kMaxValue = kMall
};

// Supported window anchor element.
// These values are deserialized from Growth Campaign, so entries should not
// be renumbered and numeric values should never be reused.
enum class WindowAnchorType {
  kCaptionButtonContainer = 0,
  kWindowBounds = 1,
  kMaxValue = kWindowBounds
};

// These values are deserialized from Growth Campaign, so entries should not
// be renumbered and numeric values should never be reused.
enum class TriggerType {
  // TODO: b/340950978 - Remove when pass the trigger in GetCampaignsBySlot().
  kUnSpecified = -1,
  kAppOpened = 0,
  kCampaignsLoaded = 1,
  kEvent = 2,
  kDelayedOneShotTimer = 3,
  kMaxValue = kDelayedOneShotTimer
};

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH) Trigger {
 public:
  explicit Trigger(TriggerType type);
  ~Trigger();

  TriggerType type;

  // A list of `events` used for `kEvent` trigger. It is considered matched if
  // any of the `events` matches with any of the event name in the
  // `triggerEvents` in the `TriggerTargeting`.
  std::vector<std::string> events;
};

// Dictionary of supported targetings. For example:
// {
//    "demoMode" : {...},
//    "session": {...}
// }
using Targeting = base::Value::Dict;

// List of `Targeting`.
using Targetings = base::Value::List;

// Dictionary of supported payloads. For example:
// {
//   "demoMode": {
//     "attractionLoop": {
//       "videoSrcLang1": "/asset/lang1.mp4",
//       "videoSrcLang2": "/asset/lang2.mp4"
//     }
//   }
// }
using Payload = base::Value::Dict;

// Dictionary of Campaign. For example:
// {
//    "id": 1,
//    "studyId":1,
//    "targetings": {...}
//    "payload": {...}
// }
using Campaign = base::Value::Dict;

// List of campaigns.
using Campaigns = base::Value::List;

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH)
const Payload* GetPayloadBySlot(const Campaign* campaign, Slot slot);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH)
std::optional<int> GetCampaignId(const Campaign* campaign);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH)
std::optional<int> GetCampaignGroupId(const Campaign* campaign);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH)
std::optional<int> GetStudyId(const Campaign* campaign);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH)
std::optional<bool> ShouldRegisterTrialWithTriggerEventName(
    const Campaign* campaign);

// Lists of campaigns keyed by the targeted slot. The key is the slot ID in
// string. For example:
// {
//   "0": [...]
//   "1": [...]
// }
using CampaignsPerSlot = base::Value::Dict;

Campaigns* GetMutableCampaignsBySlot(CampaignsPerSlot* campaigns_per_slot,
                                     Slot slot);

const Campaigns* GetCampaignsBySlot(const CampaignsPerSlot* campaigns_per_slot,
                                    Slot slot);

const Targetings* GetTargetings(const Campaign* campaign);

const Payload* GetPayloadBySlot(const Campaign* campaign, Slot slot);

class TargetingBase {
 public:
  TargetingBase(const Targeting* targeting_dict, const char* targeting_path);
  TargetingBase(const TargetingBase&) = delete;
  TargetingBase& operator=(const TargetingBase) = delete;
  ~TargetingBase();

  // True if the specific targeting (e.g: demoMode) was found in the targeting
  // dictionary. The campaign will be selected if the targeted criteria is not
  // found and defer to the next criteria matching.
  bool IsValid() const;

 protected:
  const base::Value::List* GetListCriteria(const char* path_suffix) const;
  const std::optional<bool> GetBoolCriteria(const char* path_suffix) const;
  const std::optional<int> GetIntCriteria(const char* path_suffix) const;
  const std::string* GetStringCriteria(const char* path_suffix) const;
  const base::Value::Dict* GetDictCriteria(const char* path_suffix) const;

 private:
  const std::string GetCriteriaPath(const char* path_suffix) const;

  // The dictionary that contains targeting definition. Owned by
  // `CampaignsManager`.
  raw_ptr<const Targeting> targeting_;
  // The targeting path.
  const char* targeting_path_;
};

// Demo mode targeting. For example:
// {
//   "retailers": ["bb", "bsb"];
//   "storeIds": ["2", "4", "6"],
//   "country": ["US"],
//   "capability": {
//     "isFeatureAwareDevice": false,
//     "isCloudGamingDevice": true,
//   }
// }
class DemoModeTargeting : public TargetingBase {
 public:
  explicit DemoModeTargeting(const Targeting* targeting_dict);
  DemoModeTargeting(const DemoModeTargeting&) = delete;
  DemoModeTargeting& operator=(const DemoModeTargeting) = delete;
  ~DemoModeTargeting();

  const base::Value::List* GetStoreIds() const;
  const base::Value::List* GetRetailers() const;
  const base::Value::List* GetCountries() const;
  const std::optional<base::Version> GetAppMinVersion() const;
  const std::optional<base::Version> GetAppMaxVersion() const;
  const std::optional<bool> TargetCloudGamingDevice() const;
  const std::optional<bool> TargetFeatureAwareDevice() const;
};

// Wrapper around time window targeting dictionary.
//
// The structure looks like:
// {
//   "start": 1697046365,
//   "end": 1697046598
// }
//
// Start and end are the number of seconds since epoch in UTC.
class TimeWindowTargeting {
 public:
  explicit TimeWindowTargeting(const base::Value::Dict* time_window_dict);
  TimeWindowTargeting(const TimeWindowTargeting&) = delete;
  TimeWindowTargeting& operator=(const TimeWindowTargeting) = delete;
  ~TimeWindowTargeting();

  const base::Time GetStartTime() const;
  const base::Time GetEndTime() const;

 private:
  raw_ptr<const base::Value::Dict> time_window_dict_;
};

// Wrapper around number range targeting dictionary.
//
// The structure looks like:
// {
//   "start": 3,
//   "end": 5
// }
class NumberRangeTargeting {
 public:
  explicit NumberRangeTargeting(const base::Value::Dict* number_range_dict);
  NumberRangeTargeting(const NumberRangeTargeting&) = delete;
  NumberRangeTargeting& operator=(const NumberRangeTargeting) = delete;
  ~NumberRangeTargeting();

  const std::optional<int> GetStart() const;
  const std::optional<int> GetEnd() const;

 private:
  raw_ptr<const base::Value::Dict> number_range_dict_;
};

// Wrapper around Device targeting dictionary. The structure looks like:
// {
//   "locales": ["en-US", "zh-CN"];
//   "milestone": {
//      "min": 117,
//      "max": 120
//   }
// }
class DeviceTargeting : public TargetingBase {
 public:
  explicit DeviceTargeting(const Targeting* targeting_dict);
  DeviceTargeting(const DeviceTargeting&) = delete;
  DeviceTargeting& operator=(const DeviceTargeting) = delete;
  ~DeviceTargeting();

  const base::Value::List* GetLocales() const;
  const base::Value::List* GetUserLocales() const;
  const base::Value::List* GetIncludedCountries() const;
  const base::Value::List* GetExcludedCountries() const;
  const std::optional<int> GetMinMilestone() const;
  const std::optional<int> GetMaxMilestone() const;
  const std::optional<base::Version> GetMinVersion() const;
  const std::optional<base::Version> GetMaxVersion() const;
  const std::optional<bool> GetFeatureAwareDevice() const;
  std::unique_ptr<TimeWindowTargeting> GetRegisteredTime() const;
  const std::unique_ptr<NumberRangeTargeting> GetDeviceAge() const;
};

// Wrapper around session targeting dictionary.
//
// The structure looks like:
// {
//   "session": {
//      "experimentTag": [...]
//   }
// }
class SessionTargeting : public TargetingBase {
 public:
  explicit SessionTargeting(const Targeting* targeting_dict);
  SessionTargeting(const SessionTargeting&) = delete;
  SessionTargeting& operator=(const SessionTargeting) = delete;
  ~SessionTargeting();

  std::optional<const base::Feature*> GetFeature() const;
  const base::Value::List* GetExperimentTags() const;

  std::optional<bool> GetMinorUser() const;
  std::optional<bool> GetIsOwner() const;
};

// Wrapper around app targeting dictionary.
//
// The structure looks like:
// {
//   "appId": "app_id",
// }
class AppTargeting {
 public:
  explicit AppTargeting(const base::Value::Dict* app);
  AppTargeting(const AppTargeting&) = delete;
  AppTargeting& operator=(const AppTargeting) = delete;
  ~AppTargeting();

  const std::string* GetAppId() const;

 private:
  raw_ptr<const base::Value::Dict> app_dict_;
};

// Wrapper around events targeting dictionary.
//
// The structure looks like:
// {
//   "events": {
//     // A list of list of conditions to meet.
//     // The inside list condition is logic OR.
//     // The outer list condition is logic AND.
//     // conditions_met = (A || B || C) && D;
//     "conditions": [// Thsese two conditions are logic AND.
//       [ // These three conditions are logic OR.
//         "name:A;comparator:==2;window:365;storage:365",
//         "name:B;comparator:==5;window:365;storage:365",
//         "name:C;comparator:==8;window:365;storage:365"
//       ],
//       [
//         "name:D;comparator:<3;window:365;storage:365"
//       ]
//     ]
//   }
// }
class EventsTargeting {
 public:
  explicit EventsTargeting(const base::Value::Dict* config);
  EventsTargeting(const EventsTargeting&) = delete;
  EventsTargeting& operator=(const EventsTargeting) = delete;
  ~EventsTargeting();

  int GetImpressionCap() const;
  int GetDismissalCap() const;
  std::optional<int> GetGroupImpressionCap() const;
  std::optional<int> GetGroupDismissalCap() const;
  const base::Value::List* GetEventsConditions() const;

 private:
  raw_ptr<const base::Value::Dict> config_dict_;
};

// Wrapper around trigger targeting dictionary.
//
// The structure looks like:
// {
//   "triggerType": 0,
//   "triggerEvents": ["a", "b"]
// }
class TriggerTargeting {
 public:
  explicit TriggerTargeting(const base::Value::Dict* app);
  TriggerTargeting(const TriggerTargeting&) = delete;
  TriggerTargeting& operator=(const TriggerTargeting) = delete;
  ~TriggerTargeting();

  std::optional<int> GetTriggerType() const;
  const base::Value::List* GetTriggerEvents() const;

 private:
  raw_ptr<const base::Value::Dict> trigger_dict_;
};

// Wrapper around runtime targeting dictionary.
//
// The structure looks like:
// {
//   "runtime": {
//      "schedulings": [...]
//      "appsOpend": [...]
//      "triggers": [...]
//   }
// }
class RuntimeTargeting : public TargetingBase {
 public:
  explicit RuntimeTargeting(const Targeting* targeting_dict);
  RuntimeTargeting(const RuntimeTargeting&) = delete;
  RuntimeTargeting& operator=(const RuntimeTargeting) = delete;
  ~RuntimeTargeting();

  const std::vector<std::unique_ptr<TimeWindowTargeting>> GetSchedulings()
      const;

  // Returns a list of apps to be matched against the current opened app.
  const std::vector<std::unique_ptr<AppTargeting>> GetAppsOpened() const;

  const std::vector<std::string> GetActiveUrlRegexes() const;

  std::unique_ptr<EventsTargeting> GetEventsConfig() const;

  // Returns a list of triggers against the current trigger, e.g. `kAppOpened`.
  const std::vector<std::unique_ptr<TriggerTargeting>> GetTriggers() const;

  const base::Value::List* GetUserPrefTargetings() const;

  std::unique_ptr<AppTargeting> GetHotseatAppIcon() const;
};

// Wrapper around the action dictionary for performing an action, including
// action type and action params.
// For example:
// {
//   "action": {
//     "type": 3,
//     "params": {
//       "url": "https://www.google.com",
//       "disposition": 0
//     }
//   }
// }
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH) Action {
 public:
  explicit Action(const base::Value::Dict* action_dict);
  Action(const Action&) = delete;
  Action& operator=(const Action) = delete;
  ~Action();

  std::optional<growth::ActionType> GetActionType() const;
  const base::Value::Dict* GetParams() const;

  raw_ptr<const base::Value::Dict> action_dict_;
};

// Wrapper around anchor.
//
// The structure looks like:
// {
//   "activeAppWindowAnchorType": 0  // CAPTION_BUTTON_CONTAINER
// }
// TODO(b/329698643): Consider moving to nudge controller if Anchor is not used
// by other surfaces.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH) Anchor {
 public:
  explicit Anchor(const base::Value::Dict* anchor_dict);
  Anchor(const Anchor&) = delete;
  Anchor& operator=(const Anchor) = delete;
  ~Anchor();

  const std::optional<WindowAnchorType> GetActiveAppWindowAnchorType() const;
  const std::string* GetShelfAppButtonId() const;

 private:
  raw_ptr<const base::Value::Dict> anchor_dict_;
};

// Wrapper around image dictionary.
//
// The structure looks like:
// {
//  "builtInImage": 0
// }
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH) Image {
 public:
  explicit Image(const base::Value::Dict* image_dict);
  Image(const Image&) = delete;
  Image& operator=(const Image) = delete;
  ~Image();

  const gfx::Image* GetImage() const;

 private:
  // Get built in icon based on the given image data.
  const gfx::Image* GetBuiltInImage() const;

  raw_ptr<const base::Value::Dict> image_dict_;
};

// Wrapper around vector icon dictionary.
//
// The structure looks like:
// {
//  "builtVectorIcon": 0
// }
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH) VectorIcon {
 public:
  explicit VectorIcon(const base::Value::Dict* vector_icon_dict);
  VectorIcon(const VectorIcon&) = delete;
  VectorIcon& operator=(const VectorIcon) = delete;
  ~VectorIcon();

  const gfx::VectorIcon* GetVectorIcon() const;

 private:
  // Get built in icon based on the given image data.
  const gfx::VectorIcon* GetBuiltInVectorIcon() const;

  raw_ptr<const base::Value::Dict> vector_icon_dict_;
};

// Wrapper around image model dictionary.
//
// The structure looks like:
// {
//   "image": {
//    "builtInImage": 0
//   }
// }
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH) ImageModel {
 public:
  explicit ImageModel(const base::Value::Dict* image_model_dict);
  ImageModel(const Image&) = delete;
  ImageModel& operator=(const ImageModel) = delete;
  ~ImageModel();

  const std::optional<ui::ImageModel> GetImageModel() const;

 private:
  // Get built in icon based on the given image data.
  // If given data is referring to an image, the image will be resized to 60 *
  // 60 so it can be used in the nudge.
  // TODO: b/340945779 - consider moving the resize logic to
  // `ShowNudgeActionPerformer`.
  const std::optional<ui::ImageModel> GetBuiltInImageModel() const;

  raw_ptr<const base::Value::Dict> image_model_dict_;
};

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MODEL_H_
