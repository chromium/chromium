// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/device_capabilities_impl.h"

#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

namespace {

const char kSampleDictionaryCapability[] =
    "{"
    "  \"dummy_field_bool\": true,"
    "  \"dummy_field_int\": 99"
    "}";

void GetSampleDefaultCapability(std::string* key, base::Value* init_value);
void TestBasicOperations(DeviceCapabilities* capabilities);

// Simple capability manager that implements the Validator interface. Either
// accepts or rejects all proposed changes based on |accept_changes| constructor
// argument.
class FakeCapabilityManagerSimple : public DeviceCapabilities::Validator {
 public:
  // Registers itself as Validator in constructor. If init_value is not null,
  // the capability gets initialized to that value. Else capability remains
  // untouched.
  FakeCapabilityManagerSimple(DeviceCapabilities* capabilities,
                              const std::string& key,
                              base::Value init_value,
                              bool accept_changes,
                              bool validate_private)
      : DeviceCapabilities::Validator(capabilities),
        key_(key),
        accept_changes_(accept_changes),
        validate_private_(validate_private) {
    capabilities->Register(key, this);
    if (!init_value.is_none()) {
      if (validate_private_) {
        SetPrivateValidatedValue(key, std::move(init_value));
      } else {
        SetPublicValidatedValue(key, std::move(init_value));
      }
    }
  }

  // Unregisters itself as Validator.
  ~FakeCapabilityManagerSimple() override {
    capabilities()->Unregister(key_, this);
  }

  void Validate(const std::string& path, base::Value proposed_value) override {
    ASSERT_EQ(path.find(key_), 0ul);
    if (!accept_changes_)
      return;
    if (validate_private_) {
      SetPrivateValidatedValue(path, std::move(proposed_value));
    } else {
      SetPublicValidatedValue(path, std::move(proposed_value));
    }
  }

 private:
  const std::string key_;
  const bool accept_changes_;
  const bool validate_private_;
};

// Used to test that capabilities/validator can be read and written in
// Validate() without encountering deadlocks/unexpected behavior.
class FakeCapabilityManagerComplex : public DeviceCapabilities::Validator {
 public:
  FakeCapabilityManagerComplex(DeviceCapabilities* capabilities,
                               const std::string& key)
      : DeviceCapabilities::Validator(capabilities), key_(key) {
    capabilities->Register(key, this);
  }

  // Unregisters itself as Validator.
  ~FakeCapabilityManagerComplex() override {
    capabilities()->Unregister(key_, this);
  }

  // Runs TestBasicOperations().
  void Validate(const std::string& path, base::Value proposed_value) override {
    TestBasicOperations(capabilities());
  }

 private:
  const std::string key_;
};

// Used to test that capabilities/validators can be read and written in
// OnCapabilitiesChanged() without encountering deadlocks/unexpected behavior.
class FakeCapabilitiesObserver : public DeviceCapabilities::Observer {
 public:
  explicit FakeCapabilitiesObserver(DeviceCapabilities* capabilities)
      : capabilities_(capabilities), removed_as_observer(false) {
    capabilities_->AddCapabilitiesObserver(this);
  }

  ~FakeCapabilitiesObserver() override {
    if (!removed_as_observer)
      capabilities_->RemoveCapabilitiesObserver(this);
  }

  // Runs TestBasicOperations().
  void OnCapabilitiesChanged(const std::string& path) override {
    TestBasicOperations(capabilities_);
    // To prevent infinite loop of SetCapability() -> OnCapabilitiesChanged()
    // -> SetCapability() -> OnCapabilitiesChanged() etc.
    capabilities_->RemoveCapabilitiesObserver(this);
    removed_as_observer = true;
  }

 private:
  DeviceCapabilities* const capabilities_;
  bool removed_as_observer;
};

// Used to test that OnCapabilitiesChanged() is called when capabilities are
// modified
class MockCapabilitiesObserver : public DeviceCapabilities::Observer {
 public:
  MockCapabilitiesObserver() {}

  MockCapabilitiesObserver(const MockCapabilitiesObserver&) = delete;
  MockCapabilitiesObserver& operator=(const MockCapabilitiesObserver&) = delete;

  ~MockCapabilitiesObserver() override {}

  MOCK_METHOD1(OnCapabilitiesChanged, void(const std::string& path));
};

// Test fixtures needs an example default capability to test DeviceCapabilities
// methods. Gets a sample key and initial value.
void GetSampleDefaultCapability(std::string* key, base::Value* init_value) {
  DCHECK(key);
  DCHECK(init_value);
  *key = DeviceCapabilities::kKeyBluetoothSupported;
  *init_value = base::Value(true);
}

// For test fixtures that test dynamic capabilities, gets a sample key
// and initial value.
void GetSampleDynamicCapability(std::string* key, base::Value* init_value) {
  DCHECK(key);
  DCHECK(init_value);
  *key = "dummy_dynamic_key";
  *init_value = base::Value(99);
}

// Gets a value for sample default capability different from |init_value|
// returned in GetSampleDefaultCapability(). Must be of same type as
// |init_value| of course.
base::Value GetSampleDefaultCapabilityNewValue() {
  return base::Value(false);
}

// Gets a value for sample dynamic capability different from |init_value|
// returned in GetSampleDynamicCapability(). Must be of same type as
// |init_value| of course.
base::Value GetSampleDynamicCapabilityNewValue() {
  return base::Value(100);
}

// Tests that |json| string matches contents of a DictionaryValue with one entry
// specified by |key| and |value|.
bool JsonStringEquals(const std::string& json,
                      const std::string& key,
                      const base::Value& value) {
  return base::WriteJson(base::Value::Dict().Set(key, value.Clone())) == json;
}

// The function runs through the set of basic operations of DeviceCapabilities.
// Register validator for sample default capability, reads capability, writes
// capability, and unregister validator. After it has completed, use
// AssertBasicOperationsSuccessful() to ensure that all operations completed
// successfully. Sample default capability should not be added or registered in
// class before this function is called.
void TestBasicOperations(DeviceCapabilities* capabilities) {
  std::string key;
  base::Value init_value;
  GetSampleDefaultCapability(&key, &init_value);

  ASSERT_TRUE(capabilities->GetCapability(key).is_none());
  ASSERT_FALSE(capabilities->GetValidator(key));

  // Register and write capability
  FakeCapabilityManagerSimple* manager(new FakeCapabilityManagerSimple(
      capabilities, key, init_value.Clone(), true, false));
  // Read Validator
  EXPECT_EQ(capabilities->GetValidator(key), manager);
  // Read Capability
  EXPECT_EQ(capabilities->GetCapability(key), init_value);
  // Unregister
  delete manager;

  // Write capability again. Provides way of checking that this function
  // ran and was successful.
  base::Value new_value = GetSampleDefaultCapabilityNewValue();
  capabilities->SetCapability(key, std::move(new_value));
}

// See TestBasicOperations() comment.
void AssertBasicOperationsSuccessful(const DeviceCapabilities* capabilities) {
  base::RunLoop().RunUntilIdle();
  std::string key;
  base::Value init_value;
  GetSampleDefaultCapability(&key, &init_value);
  base::Value value = capabilities->GetCapability(key);
  base::Value new_value = GetSampleDefaultCapabilityNewValue();
  EXPECT_EQ(value, new_value);
}

}  // namespace

class DeviceCapabilitiesImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    capabilities_ = DeviceCapabilities::Create();
    mock_capabilities_observer_.reset(new MockCapabilitiesObserver());
    capabilities_->AddCapabilitiesObserver(mock_capabilities_observer_.get());

    // We set the default gmock expected calls to any so that tests must
    // 'opt in' to checking the calls rather than 'opt out'. This avoids having
    // to add explicit calls in test cases that don't care in order to prevent
    // lots of useless mock warnings.
    EXPECT_CALL(*mock_capabilities_observer_, OnCapabilitiesChanged(testing::_))
        .Times(testing::AnyNumber());
  }

  void TearDown() override {
    capabilities_->RemoveCapabilitiesObserver(
        mock_capabilities_observer_.get());
    mock_capabilities_observer_.reset();
    capabilities_.reset();
  }

  DeviceCapabilities* capabilities() const { return capabilities_.get(); }

  MockCapabilitiesObserver* capabilities_observer() const {
    return mock_capabilities_observer_.get();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<DeviceCapabilities> capabilities_;
  std::unique_ptr<MockCapabilitiesObserver> mock_capabilities_observer_;
};

// Tests that class is in correct state after Create().
TEST_F(DeviceCapabilitiesImplTest, Create) {
  std::string empty_dict_string;
  base::JSONWriter::Write(base::Value(base::Value::Type::DICT),
                          &empty_dict_string);
  EXPECT_EQ(capabilities()->GetAllData()->json_string(), empty_dict_string);
  EXPECT_TRUE(capabilities()->GetAllData()->dictionary().empty());
}

// Tests Register() of a default capability.
TEST_F(DeviceCapabilitiesImplTest, Register) {
  std::string key;
  base::Value init_value;
  GetSampleDefaultCapability(&key, &init_value);

  EXPECT_CALL(*capabilities_observer(), OnCapabilitiesChanged(key)).Times(0);
  FakeCapabilityManagerSimple manager(capabilities(), key, base::Value(), true,
                                      false);

  EXPECT_EQ(capabilities()->GetValidator(key), &manager);
  std::string empty_dict_string;
  base::JSONWriter::Write(base::Value(base::Value::Type::DICT),
                          &empty_dict_string);
  EXPECT_EQ(capabilities()->GetAllData()->json_string(), empty_dict_string);
  EXPECT_TRUE(capabilities()->GetCapability(key).is_none());
}

// Tests Unregister() of a default capability.
TEST_F(DeviceCapabilitiesImplTest, Unregister) {
  std::string key;
  base::Value init_value;
  GetSampleDefaultCapability(&key, &init_value);

  EXPECT_CALL(*capabilities_observer(), OnCapabilitiesChanged(key)).Times(0);
  FakeCapabilityManagerSimple* manager = new FakeCapabilityManagerSimple(
      capabilities(), key, base::Value(), true, false);

  delete manager;

  EXPECT_FALSE(capabilities()->GetValidator(key));
  std::string empty_dict_string;
  base::JSONWriter::Write(base::Value(base::Value::Type::DICT),
                          &empty_dict_string);
  EXPECT_EQ(capabilities()->GetAllData()->json_string(), empty_dict_string);
  EXPECT_TRUE(capabilities()->GetCapability(key).is_none());
}

// Tests GetCapability() and updating the value through SetCapability().
TEST_F(DeviceCapabilitiesImplTest, GetCapabilityAndSetCapability) {
  std::string key;
  base::Value init_value;
  GetSampleDefaultCapability(&key, &init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key, init_value.Clone(),
                                      true, false);

  EXPECT_EQ(capabilities()->GetCapability(key), init_value);

  base::Value new_value = GetSampleDefaultCapabilityNewValue();
  capabilities()->SetCapability(key, new_value.Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(capabilities()->GetCapability(key), new_value);
}

// Tests BluetoothSupported() and updating this value through SetCapability().
TEST_F(DeviceCapabilitiesImplTest, BluetoothSupportedAndSetCapability) {
  FakeCapabilityManagerSimple manager(
      capabilities(), DeviceCapabilities::kKeyBluetoothSupported,
      base::Value(true), true, false);

  EXPECT_TRUE(capabilities()->BluetoothSupported());
  capabilities()->SetCapability(DeviceCapabilities::kKeyBluetoothSupported,
                                base::Value(false));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(capabilities()->BluetoothSupported());
}

// Tests DisplaySupported() and updating this value through SetCapability().
TEST_F(DeviceCapabilitiesImplTest, DisplaySupportedAndSetCapability) {
  FakeCapabilityManagerSimple manager(capabilities(),
                                      DeviceCapabilities::kKeyDisplaySupported,
                                      base::Value(true), true, false);

  EXPECT_TRUE(capabilities()->DisplaySupported());
  capabilities()->SetCapability(DeviceCapabilities::kKeyDisplaySupported,
                                base::Value(false));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(capabilities()->DisplaySupported());
}

// Tests HiResAudioSupported() and updating this value through SetCapability()
TEST_F(DeviceCapabilitiesImplTest, HiResAudioSupportedAndSetCapability) {
  FakeCapabilityManagerSimple manager(
      capabilities(), DeviceCapabilities::kKeyHiResAudioSupported,
      base::Value(true), true, false);

  EXPECT_TRUE(capabilities()->HiResAudioSupported());
  capabilities()->SetCapability(DeviceCapabilities::kKeyHiResAudioSupported,
                                base::Value(false));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(capabilities()->HiResAudioSupported());
}

// Tests AssistantSupported() and updating this value through SetCapability()
TEST_F(DeviceCapabilitiesImplTest, AssistantSupportedAndSetCapability) {
  FakeCapabilityManagerSimple manager(
      capabilities(), DeviceCapabilities::kKeyAssistantSupported,
      base::Value(true), true, false);

  EXPECT_TRUE(capabilities()->AssistantSupported());
  capabilities()->SetCapability(DeviceCapabilities::kKeyAssistantSupported,
                                base::Value(false));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(capabilities()->AssistantSupported());
}

// Tests SetCapability() for a default capability when the capability's manager
// rejects the proposed change.
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityInvalid) {
  std::string key;
  base::Value init_value;
  GetSampleDefaultCapability(&key, &init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key, init_value.Clone(),
                                      false, false);

  capabilities()->SetCapability(key, GetSampleDefaultCapabilityNewValue());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(capabilities()->GetCapability(key), init_value);
}

// Test that SetCapability() updates the capabilities string correctly
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityUpdatesString) {
  std::string key;
  base::Value init_value;
  GetSampleDefaultCapability(&key, &init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key, init_value.Clone(),
                                      true, false);

  EXPECT_TRUE(JsonStringEquals(capabilities()->GetAllData()->json_string(), key,
                               init_value));

  base::Value new_value = GetSampleDefaultCapabilityNewValue();
  capabilities()->SetCapability(key, new_value.Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(JsonStringEquals(capabilities()->GetAllData()->json_string(), key,
                               new_value));
}

// Tests that GetPublicData() does not include private capabilities
TEST_F(DeviceCapabilitiesImplTest, SetPublicPrivateCapabilities) {
  std::string key_private = "private";
  std::string key_public = "public";
  base::Value init_value(true);

  // Dictionary of only public values.
  base::Value::Dict public_dict;
  public_dict.Set(key_public, init_value.Clone());
  // Dictionary of public and private values.
  base::Value::Dict full_dict;
  full_dict.Set(key_public, init_value.Clone());
  full_dict.Set(key_private, init_value.Clone());

  FakeCapabilityManagerSimple public_manager(capabilities(), key_public,
                                             init_value.Clone(), true, false);
  FakeCapabilityManagerSimple private_manager(capabilities(), key_private,
                                              init_value.Clone(), true, true);

  EXPECT_EQ(capabilities()->GetAllData()->dictionary(), full_dict);
  EXPECT_EQ(capabilities()->GetPublicData()->dictionary(), public_dict);
}

// Tests that SetCapability() defaults to making a capability public
TEST_F(DeviceCapabilitiesImplTest, NoValidatorDefaultsToPublicCapability) {
  std::string key_private = "private";
  std::string key_public = "public";
  base::Value init_value(true);

  // Dictionary of only public values.
  base::Value::Dict public_dict;
  public_dict.Set(key_public, init_value.Clone());
  // Dictionary of public and private values.
  base::Value::Dict full_dict;
  full_dict.Set(key_public, init_value.Clone());
  full_dict.Set(key_private, init_value.Clone());

  // We will not create a validator for the public capability; instead we will
  // set the capability directly. It will be registered as a public capability.
  capabilities()->SetCapability(key_public, init_value.Clone());

  FakeCapabilityManagerSimple private_manager(capabilities(), key_private,
                                              init_value.Clone(), true, true);

  EXPECT_EQ(capabilities()->GetAllData()->dictionary(), full_dict);
  EXPECT_EQ(capabilities()->GetPublicData()->dictionary(), public_dict);
}

// Test that SetCapability() notifies Observers when the capability's value
// changes
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityNotifiesObservers) {
  std::string key;
  base::Value init_value;
  GetSampleDefaultCapability(&key, &init_value);

  EXPECT_CALL(*capabilities_observer(), OnCapabilitiesChanged(key)).Times(3);

  // 1st call (register)
  FakeCapabilityManagerSimple manager(capabilities(), key, init_value.Clone(),
                                      true, false);

  // 2nd call
  capabilities()->SetCapability(key, GetSampleDefaultCapabilityNewValue());

  // Observer should not get called when value does not change
  capabilities()->SetCapability(key, GetSampleDefaultCapabilityNewValue());

  // 3rd call
  capabilities()->SetCapability(key, std::move(init_value));
  base::RunLoop().RunUntilIdle();
}

// Test that SetCapability() notifies Observers when a private capability's
// value changes
TEST_F(DeviceCapabilitiesImplTest, SetPrivateCapabilityNotifiesObservers) {
  std::string key;
  base::Value init_value;
  GetSampleDefaultCapability(&key, &init_value);

  EXPECT_CALL(*capabilities_observer(), OnCapabilitiesChanged(key)).Times(3);

  // 1st call (register), this manager validates and sets the capability
  // privately.
  FakeCapabilityManagerSimple manager(capabilities(), key, init_value.Clone(),
                                      true, true);

  // 2nd call
  capabilities()->SetCapability(key, GetSampleDefaultCapabilityNewValue());

  // Observer should not get called when value does not change
  capabilities()->SetCapability(key, GetSampleDefaultCapabilityNewValue());

  // 3rd call
  capabilities()->SetCapability(key, std::move(init_value));
  base::RunLoop().RunUntilIdle();
}

// Test adding dynamic capabilities
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityDynamic) {
  std::string key;
  base::Value init_value;
  GetSampleDynamicCapability(&key, &init_value);

  ASSERT_TRUE(capabilities()->GetCapability(key).is_none());
  capabilities()->SetCapability(key, init_value.Clone());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(capabilities()->GetCapability(key), init_value);
  EXPECT_TRUE(JsonStringEquals(capabilities()->GetAllData()->json_string(), key,
                               init_value));

  base::Value new_value = GetSampleDynamicCapabilityNewValue();
  capabilities()->SetCapability(key, new_value.Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(capabilities()->GetCapability(key), new_value);
  EXPECT_TRUE(JsonStringEquals(capabilities()->GetAllData()->json_string(), key,
                               new_value));
}

// Tests that SetCapability() works with expanded paths when there is a
// capability of type DictionaryValue.
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityDictionary) {
  std::string key("dummy_dictionary_key");
  auto init_value = base::JSONReader::Read(kSampleDictionaryCapability);
  ASSERT_TRUE(init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key,
                                      std::move(*init_value), true, false);

  capabilities()->SetCapability("dummy_dictionary_key.dummy_field_bool",
                                base::Value(false));
  base::RunLoop().RunUntilIdle();
  base::Value value =
      capabilities()->GetCapability("dummy_dictionary_key.dummy_field_bool");
  ASSERT_TRUE(value.is_bool());
  EXPECT_FALSE(value.GetBool());

  capabilities()->SetCapability("dummy_dictionary_key.dummy_field_int",
                                base::Value(100));
  base::RunLoop().RunUntilIdle();
  value = capabilities()->GetCapability("dummy_dictionary_key.dummy_field_int");
  ASSERT_TRUE(value.is_int());
  EXPECT_EQ(value.GetInt(), 100);
}

// Tests that SetCapability() works with expanded paths when there is a
// capability of type DictionaryValue and invalid changes are proposed.
TEST_F(DeviceCapabilitiesImplTest, SetCapabilityDictionaryInvalid) {
  std::string key("dummy_dictionary_key");
  auto init_value = base::JSONReader::Read(kSampleDictionaryCapability);
  ASSERT_TRUE(init_value);
  FakeCapabilityManagerSimple manager(capabilities(), key,
                                      std::move(*init_value), false, false);

  capabilities()->SetCapability("dummy_dictionary_key.dummy_field_bool",
                                base::Value(false));
  base::RunLoop().RunUntilIdle();
  base::Value value =
      capabilities()->GetCapability("dummy_dictionary_key.dummy_field_bool");
  ASSERT_TRUE(value.is_bool());
  EXPECT_TRUE(value.GetBool());

  capabilities()->SetCapability("dummy_dictionary_key.dummy_field_int",
                                base::Value(100));
  base::RunLoop().RunUntilIdle();
  value = capabilities()->GetCapability("dummy_dictionary_key.dummy_field_int");
  ASSERT_TRUE(value.is_int());
  EXPECT_EQ(value.GetInt(), 99);
}

// Test MergeDictionary.
TEST_F(DeviceCapabilitiesImplTest, MergeDictionary) {
  std::optional<base::Value::Dict> deserialized_value =
      base::JSONReader::ReadDict(kSampleDictionaryCapability);
  ASSERT_TRUE(deserialized_value);

  capabilities()->MergeDictionary(*deserialized_value);
  base::RunLoop().RunUntilIdle();

  // First make sure that capabilities get created if they do not exist
  base::Value value = capabilities()->GetCapability("dummy_field_bool");
  ASSERT_TRUE(value.is_bool());
  EXPECT_TRUE(value.GetBool());

  value = capabilities()->GetCapability("dummy_field_int");
  ASSERT_TRUE(value.is_int());
  EXPECT_EQ(value.GetInt(), 99);

  // Now just update one of the fields. Make sure the updated value is changed
  // in DeviceCapabilities and the other field remains untouched.
  deserialized_value->Set("dummy_field_int", 100);
  ASSERT_TRUE(deserialized_value->Remove("dummy_field_bool"));

  capabilities()->MergeDictionary(*deserialized_value);
  base::RunLoop().RunUntilIdle();

  value = capabilities()->GetCapability("dummy_field_bool");
  ASSERT_TRUE(value.is_bool());
  EXPECT_TRUE(value.GetBool());

  value = capabilities()->GetCapability("dummy_field_int");
  ASSERT_TRUE(value.is_int());
  EXPECT_EQ(value.GetInt(), 100);
}

// Tests that it is safe to call DeviceCapabilities methods in
// an Observer's OnCapabilitiesChanged() implementation safely with correct
// behavior and without deadlocking.
TEST_F(DeviceCapabilitiesImplTest, OnCapabilitiesChangedSafe) {
  FakeCapabilitiesObserver observer(capabilities());

  // Trigger FakeCapabilitiesObserver::OnCapabilitiesChanged()
  capabilities()->SetCapability("dummy_trigger_key", base::Value(true));
  base::RunLoop().RunUntilIdle();

  // Check that FakeCapabilitiesObserver::OnCapabilitiesChanged() ran and that
  // behavior was successful
  AssertBasicOperationsSuccessful(capabilities());
}

// Tests that it is safe to call DeviceCapabilities methods in a Validator's
// Validate() implementation safely with correct behavior and without
// deadlocking.
TEST_F(DeviceCapabilitiesImplTest, ValidateSafe) {
  FakeCapabilityManagerComplex manager(capabilities(), "dummy_validate_key");

  // Trigger FakeCapabilityManagerComplex::Validate()
  capabilities()->SetCapability("dummy_validate_key", base::Value(true));
  base::RunLoop().RunUntilIdle();

  // Check that FakeCapabilityManagerComplex::Validate() ran and that behavior
  // was successful
  AssertBasicOperationsSuccessful(capabilities());
}

}  // namespace chromecast
