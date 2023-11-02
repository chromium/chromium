// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_REGISTRATION_INFO_H_
#define COMPONENTS_GCM_DRIVER_REGISTRATION_INFO_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace gcm  {

// Encapsulates the information needed to register with the server.
struct RegistrationInfo : public base::RefCounted<RegistrationInfo> {
  enum RegistrationType {
    GCM_REGISTRATION,
    INSTANCE_ID_TOKEN
  };

  // Returns the appropriate RegistrationInfo instance based on the serialized
  // key and value.
  // |registration_id| can be NULL if no interest to it.
  static scoped_refptr<RegistrationInfo> BuildFromString(
      const std::string& serialized_key,
      const std::string& serialized_value,
      std::string* registration_id);

  RegistrationInfo();

  // Returns the type of the registration info.
  virtual RegistrationType GetType() const = 0;

  // For persisting to the store. Depending on the type, part of the
  // registration info is written as key. The remaining of the registration
  // info plus the registration ID are written as value.
  virtual std::string GetSerializedKey() const = 0;
  virtual std::string GetSerializedValue(
      const std::string& registration_id) const = 0;
  // |registration_id| can be NULL if it is of no interest to the caller.
  virtual bool Deserialize(const std::string& serialized_key,
                           const std::string& serialized_value,
                           std::string* registration_id) = 0;

  // Every registration is associated with an application.
  std::string app_id;
  base::Time last_validated;

 protected:
  friend class base::RefCounted<RegistrationInfo>;
  virtual ~RegistrationInfo();
};

// For GCM registration.
struct GCMRegistrationInfo final : public RegistrationInfo {
  GCMRegistrationInfo();

  // Converts from the base type;
  static const GCMRegistrationInfo* FromRegistrationInfo(
      const RegistrationInfo* registration_info);
  static GCMRegistrationInfo* FromRegistrationInfo(
      RegistrationInfo* registration_info);

  // RegistrationInfo overrides:
  RegistrationType GetType() const override;
  std::string GetSerializedKey() const override;
  std::string GetSerializedValue(
      const std::string& registration_id) const override;
  bool Deserialize(const std::string& serialized_key,
                   const std::string& serialized_value,
                   std::string* registration_id) override;

  // List of IDs of the servers that are allowed to send the messages to the
  // application. These IDs are assigned by the Google API Console.
  std::vector<std::string> sender_ids;

 private:
  ~GCMRegistrationInfo() override;
};

// For InstanceID token retrieval.
struct InstanceIDTokenInfo final : public RegistrationInfo {
  InstanceIDTokenInfo();

  // Converts from the base type;
  static const InstanceIDTokenInfo* FromRegistrationInfo(
      const RegistrationInfo* registration_info);
  static InstanceIDTokenInfo* FromRegistrationInfo(
      RegistrationInfo* registration_info);

  // RegistrationInfo overrides:
  RegistrationType GetType() const override;
  std::string GetSerializedKey() const override;
  std::string GetSerializedValue(
      const std::string& registration_id) const override;
  bool Deserialize(const std::string& serialized_key,
                   const std::string& serialized_value,
                   std::string* registration_id) override;

  // Entity that is authorized to access resources associated with the Instance
  // ID. It can be another Instance ID or a project ID assigned by the Google
  // API Console.
  std::string authorized_entity;

  // Authorized actions that the authorized entity can take.
  // E.g. for sending GCM messages, 'GCM' scope should be used.
  std::string scope;

  // Specifies TTL of retrievable token, zero value means unlimited TTL.
  // Not serialized/deserialized.
  base::TimeDelta time_to_live;

 private:
  ~InstanceIDTokenInfo() override;
};

struct RegistrationInfoComparer {
  bool operator()(const scoped_refptr<RegistrationInfo>& a,
                  const scoped_refptr<RegistrationInfo>& b) const;
};

// Collection of registration info.
// Map from RegistrationInfo instance to registration ID.
using RegistrationInfoMap = std::
    map<scoped_refptr<RegistrationInfo>, std::string, RegistrationInfoComparer>;

// Returns true if a GCM registration for |app_id| exists in |map|.
bool ExistsGCMRegistrationInMap(const RegistrationInfoMap& map,
                                const std::string& app_id);

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_REGISTRATION_INFO_H_
