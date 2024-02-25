// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_UI_DATA_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_UI_DATA_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/values.h"
#include "components/onc/onc_constants.h"

namespace ash {

// Helper for accessing and setting values in the network's UI data dictionary.
// Accessing values is done via static members that take the network as an
// argument.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkUIData {
 public:
  NetworkUIData();
  NetworkUIData(const NetworkUIData& other);
  NetworkUIData& operator=(const NetworkUIData& other);
  explicit NetworkUIData(const base::Value::Dict& dict);
  ~NetworkUIData();

  // Creates a NetworkUIData object from |onc_source|. This function is used to
  // create the "UIData" property of the Shill configuration.
  static std::unique_ptr<NetworkUIData> CreateFromONC(
      ::onc::ONCSource onc_source);

  // Returns a |user_settings_|.
  const base::Value::Dict* GetUserSettingsDictionary() const;

  // Sets |user_settings_| to the provided value.
  void SetUserSettingsDictionary(base::Value::Dict dict);

  // Returns a JSON string representing currently configured values for storing
  // in Shill.
  std::string GetAsJson() const;

  ::onc::ONCSource onc_source() const { return onc_source_; }

 private:
  std::string GetONCSourceAsString() const;

  ::onc::ONCSource onc_source_;
  std::optional<base::Value::Dict> user_settings_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_UI_DATA_H_
