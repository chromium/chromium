// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_RENDERER_EXTERNAL_CLEAR_KEY_KEY_SYSTEM_PROPERTIES_H_
#define COMPONENTS_CDM_RENDERER_EXTERNAL_CLEAR_KEY_KEY_SYSTEM_PROPERTIES_H_

#include <string>

#include "build/build_config.h"
#include "media/base/key_system_properties.h"
#include "media/media_buildflags.h"

namespace cdm {

// KeySystemProperties implementation for external Clear Key key systems.
class ExternalClearKeyProperties : public media::KeySystemProperties {
 public:
  explicit ExternalClearKeyProperties(const std::string& key_system_name);
  ~ExternalClearKeyProperties() override;

  std::string GetKeySystemName() const override;
  bool IsSupportedInitDataType(
      media::EmeInitDataType init_data_type) const override;
  media::EmeConfigRule GetEncryptionSchemeConfigRule(
      media::EncryptionScheme encryption_scheme) const override;
  media::SupportedCodecs GetSupportedCodecs() const override;
  media::EmeConfigRule GetRobustnessConfigRule(
      media::EmeMediaType media_type,
      const std::string& requested_robustness) const override;
  media::EmeSessionTypeSupport GetPersistentLicenseSessionSupport()
      const override;
  media::EmeSessionTypeSupport GetPersistentUsageRecordSessionSupport()
      const override;
  media::EmeFeatureSupport GetPersistentStateSupport() const override;
  media::EmeFeatureSupport GetDistinctiveIdentifierSupport() const override;

 private:
  const std::string key_system_name_;
};

}  // namespace cdm

#endif  // COMPONENTS_CDM_RENDERER_EXTERNAL_CLEAR_KEY_KEY_SYSTEM_PROPERTIES_H_
