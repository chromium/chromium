// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_TEST_POLICY_BUILDER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_TEST_POLICY_BUILDER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/rsa_private_key.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/policy/proto/chrome_extension_policy.pb.h"
#endif

namespace enterprise_management {
class CloudPolicySettings;
}  // namespace enterprise_management

namespace policy {

extern const uint8_t kVerificationPrivateKey[1218];

// A helper class for testing that provides a straightforward interface for
// constructing policy blobs for use in testing. NB: This uses fake data and
// hard-coded signing keys by default, so should not be used in production code.
// The signatures are generated based on different key than used in production,
// so any test using this class needs to add the command line flag
// switches::kPolicyVerificationKey to change the verification key if they
// wish the signatures provided by this class to pass validation.
class PolicyBuilder {
 public:
  // Constants used as dummy data for filling the PolicyData protobuf.
  static const char kFakeDeviceId[];
  static const char kFakeDomain[];
  static const char kFakeGaiaId[];
  static const char kFakeMachineName[];
  static const char kFakePolicyType[];
  static const int kFakePublicKeyVersion;
  static const int64_t kFakeTimestamp;
  static const char kFakeToken[];
  static const char kFakeUsername[];
  static const char kFakeServiceAccountIdentity[];

  // Creates a policy builder. The builder will have all |policy_data_| fields
  // initialized to dummy values and use the test signing keys.
  PolicyBuilder();
  PolicyBuilder(const PolicyBuilder&) = delete;
  PolicyBuilder& operator=(const PolicyBuilder&) = delete;
  virtual ~PolicyBuilder();

  // Returns a reference to the policy data protobuf being built. Note that an
  // initial policy data payload protobuf is created and filled with testing
  // values in the constructor. Note also that the public_key_version field will
  // be filled with the right values only after the Build() method call.
  enterprise_management::PolicyData& policy_data() { return *policy_data_; }
  const enterprise_management::PolicyData& policy_data() const {
    return *policy_data_;
  }
  void clear_policy_data() { policy_data_.reset(); }
  void CreatePolicyData() {
    policy_data_ = std::make_unique<enterprise_management::PolicyData>();
  }

  // Returns a reference to the policy protobuf being built. Note that the
  // fields relating to the public key, serialized policy data and signature
  // will be filled with the right values only after the Build() method call.
  enterprise_management::PolicyFetchResponse& policy() { return policy_; }
  const enterprise_management::PolicyFetchResponse& policy() const {
    return policy_;
  }

  // Use these methods for obtaining and changing the current signing key.
  // Note that, by default, a hard-coded testing signing key is used.
  std::unique_ptr<crypto::RSAPrivateKey> GetSigningKey() const;
  void SetSigningKey(const crypto::RSAPrivateKey& key);
  void SetDefaultSigningKey();
  void UnsetSigningKey();

  // Use these methods for obtaining and changing the new signing key.
  // By default, there is no new signing key.
  std::unique_ptr<crypto::RSAPrivateKey> GetNewSigningKey() const;
  void SetDefaultNewSigningKey();
  void UnsetNewSigningKey();

  // Sets the default initial signing key - the resulting policy will be signed
  // by the default signing key, and will have that key set as the
  // new_public_key field, as if it were an initial key provision.
  void SetDefaultInitialSigningKey();

  // Assembles the policy components. The resulting policy protobuf is available
  // through policy() after this call.
  virtual void Build();

  // Returns a copy of policy().
  std::unique_ptr<enterprise_management::PolicyFetchResponse> GetCopy() const;

  // Returns a binary policy blob, i.e. an encoded PolicyFetchResponse.
  std::string GetBlob() const;

  // These return hard-coded testing keys. Don't use in production!
  static std::unique_ptr<crypto::RSAPrivateKey> CreateTestSigningKey();
  static std::unique_ptr<crypto::RSAPrivateKey> CreateTestOtherSigningKey();

  // Verification signatures for the two hard-coded testing keys above. These
  // signatures are valid only for the kFakeDomain domain.
  static std::string GetTestSigningKeySignature();
  static std::string GetTestSigningKeySignatureForChild();
  static std::string GetTestOtherSigningKeySignature();

  std::vector<uint8_t> raw_signing_key() const { return raw_signing_key_; }
  std::vector<uint8_t> raw_new_signing_key() const {
    return raw_new_signing_key_;
  }

  // These methods return the public part of the corresponding signing keys,
  // using the same binary format that is used for storing the public keys in
  // the policy protobufs.
  std::vector<uint8_t> GetPublicSigningKey() const;
  std::vector<uint8_t> GetPublicNewSigningKey() const;
  static std::vector<uint8_t> GetPublicTestKey();
  static std::vector<uint8_t> GetPublicTestOtherKey();

  // Returns the Base64 encoded verification public key.
  static std::string GetEncodedPolicyVerificationKey();
  static std::string GetPublicKeyVerificationDataSignature();

  // These methods return the public part of the corresponding signing keys as a
  // string, using the same binary format that is used for storing the public
  // keys in the policy protobufs.
  std::string GetPublicSigningKeyAsString() const;
  std::string GetPublicNewSigningKeyAsString() const;
  static std::string GetPublicTestKeyAsString();
  static std::string GetPublicTestOtherKeyAsString();

  static std::vector<std::string> GetUserAffiliationIds();

  // Created using dummy data used for filling the PolicyData protobuf.
  static AccountId GetFakeAccountIdForTesting();

  void SetSignatureType(
      enterprise_management::PolicyFetchRequest::SignatureType signature_type);

 private:
  enterprise_management::PolicyFetchResponse policy_;
  std::unique_ptr<enterprise_management::PolicyData> policy_data_;

  // The keys cannot be stored in NSS. Temporary keys are not guaranteed to
  // remain in the database. Persistent keys require a persistent database,
  // which would coincide with the user's database. However, these keys are used
  // for signing the policy and don't have to coincide with the user's known
  // keys. Instead, we store the private keys as raw bytes. Where needed, a
  // temporary RSAPrivateKey is created.
  std::vector<uint8_t> raw_signing_key_;
  std::vector<uint8_t> raw_new_signing_key_;
  std::string raw_new_signing_key_signature_;

  enterprise_management::PolicyFetchRequest::SignatureType signature_type_ =
      enterprise_management::PolicyFetchRequest::SHA1_RSA;
};

// Type-parameterized PolicyBuilder extension that allows for building policy
// blobs carrying protobuf payloads.
template <typename PayloadProto>
class TypedPolicyBuilder : public PolicyBuilder {
 public:
  TypedPolicyBuilder();
  TypedPolicyBuilder(const TypedPolicyBuilder&) = delete;
  TypedPolicyBuilder& operator=(const TypedPolicyBuilder&) = delete;

  // Returns a reference to the payload protobuf being built. Note that an
  // initial payload protobuf is created in the constructor.
  PayloadProto& payload() { return *payload_; }
  const PayloadProto& payload() const { return *payload_; }
  void clear_payload() { payload_.reset(); }
  void CreatePayload() { payload_ = std::make_unique<PayloadProto>(); }

  // PolicyBuilder:
  void Build() override {
    if (payload_) {
      CHECK(payload_->SerializeToString(policy_data().mutable_policy_value()));
    }

    PolicyBuilder::Build();
  }

 private:
  std::unique_ptr<PayloadProto> payload_;
};

// PolicyBuilder extension that allows for building policy blobs carrying string
// payloads.
class StringPolicyBuilder : public PolicyBuilder {
 public:
  StringPolicyBuilder();
  StringPolicyBuilder(const StringPolicyBuilder&) = delete;
  StringPolicyBuilder& operator=(const StringPolicyBuilder&) = delete;

  void set_payload(std::string payload) { payload_ = std::move(payload); }
  const std::string& payload() const { return payload_; }
  void clear_payload() { payload_.clear(); }

  // PolicyBuilder:
  void Build() override;

 private:
  std::string payload_;
};

using UserPolicyBuilder =
    TypedPolicyBuilder<enterprise_management::CloudPolicySettings>;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
using ComponentCloudPolicyBuilder =
    TypedPolicyBuilder<enterprise_management::ExternalPolicyData>;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
using ComponentActiveDirectoryPolicyBuilder = StringPolicyBuilder;
#endif

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_TEST_POLICY_BUILDER_H_
