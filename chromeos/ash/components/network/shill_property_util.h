// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_SHILL_PROPERTY_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_SHILL_PROPERTY_UTIL_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/values.h"
#include "components/onc/onc_constants.h"

namespace ash {

class NetworkUIData;

namespace shill_property_util {

// Sets the |ssid| in |properties|.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void SetSSID(const std::string& ssid, base::Value::Dict* properties);

// Returns the SSID from |properties| in UTF-8 encoding. If |verbose_logging| is
// true, detailed DEBUG log events will be added to the device event log. If
// |unknown_encoding| != nullptr, it is set to whether the SSID is of unknown
// encoding.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::string GetSSIDFromProperties(const base::Value::Dict& properties,
                                  bool verbose_logging,
                                  bool* unknown_encoding);

// Returns the GUID (if available), SSID, or Name from |properties|. Only used
// for logging and debugging.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::string GetNetworkIdFromProperties(const base::Value::Dict& properties);

// Returns the name for the network represented by the Shill |properties|. For
// WiFi it refers to the HexSSID.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::string GetNameFromProperties(const std::string& service_path,
                                  const base::Value::Dict& properties);

// Returns the UIData specified by |value|. Returns NULL if the value cannot be
// parsed.
std::unique_ptr<NetworkUIData> GetUIDataFromValue(const base::Value& value);

// Returns the NetworkUIData parsed from the UIData property of
// |shill_dictionary|. If parsing fails or the field doesn't exist, returns
// NULL.
std::unique_ptr<NetworkUIData> GetUIDataFromProperties(
    const base::Value::Dict& shill_dictionary);

// Sets the UIData property in |shill_dictionary| to the serialization of
// |ui_data|. Sets the ONCSource property in |shill_dictionary|,
// derived from |ui_data|.
void SetUIDataAndSource(const NetworkUIData& ui_data,
                        base::Value::Dict* shill_dictionary);

// Sets the RandomMACPolicy property in |shill_dictionary|.
// This is only a temporary logic, until UI is present.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void SetRandomMACPolicy(::onc::ONCSource onc_source,
                        base::Value::Dict* shill_dictionary);

// Copy configuration properties required by Shill to identify a network in the
// format that Shill expects on writes.
// Only WiFi, VPN, Ethernet and EthernetEAP are supported. Cellular is not
// supported.
// If |properties_read_from_shill| is true, it is assumed that
// |service_properties| has the format that Shill exposes on reads, as opposed
// to property dictionaries which are sent to Shill. Returns true only if all
// required properties could be copied.
bool CopyIdentifyingProperties(const base::Value::Dict& service_properties,
                               const bool properties_read_from_shill,
                               base::Value::Dict* dest);

// Compares the identifying configuration properties of |new_properties| and
// |old_properties|, returns true if they are identical. |new_properties| must
// have the form that Shill expects on writes. |old_properties| must have the
// form that Shill exposes on reads. See also CopyIdentifyingProperties. Only
// WiFi, VPN, Ethernet and EthernetEAP are supported. Cellular is not supported.
bool DoIdentifyingPropertiesMatch(const base::Value::Dict& new_properties,
                                  const base::Value::Dict& old_properties);

// Returns false if |key| is something that should not be logged either
// because it is sensitive or noisy. Note: this is not necessarily
// comprehensive, do not use it for anything genuinely sensitive (user logs
// should always be treated as sensitive data, but sometimes they end up
// attached to public issues so this helps prevent accidents, but it should not
// be relied upon).
bool IsLoggableShillProperty(const std::string& key);

}  // namespace shill_property_util
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_SHILL_PROPERTY_UTIL_H_
