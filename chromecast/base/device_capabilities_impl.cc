// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/device_capabilities_impl.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromecast/base/serializers.h"

namespace chromecast {

namespace {

const char kPathSeparator = '.';

// Determines if a key passed to Register() is valid. No path separators can
// be present in the key and it must not be empty.
bool IsValidRegisterKey(const std::string& key) {
  return !key.empty() && key.find(kPathSeparator) == std::string::npos;
}

// Determines if a path is valid. This is true if there are no empty keys
// anywhere in the path (ex: .foo, foo., foo..bar are all invalid).
bool IsValidPath(const std::string& path) {
  return !path.empty() && *path.begin() != kPathSeparator &&
         *path.rbegin() != kPathSeparator &&
         path.find("..") == std::string::npos;
}

// Given a path, gets the first key present in the path (ex: for path "foo.bar"
// return "foo").
std::string GetFirstKey(const std::string& path) {
  std::size_t length_to_first_separator = path.find(kPathSeparator);
  return (length_to_first_separator == std::string::npos)
             ? path
             : path.substr(0, length_to_first_separator);
}

}  // namespace

// static Default Capability Keys
const char DeviceCapabilities::kKeyAssistantSupported[] = "assistant_supported";
const char DeviceCapabilities::kKeyBluetoothSupported[] = "bluetooth_supported";
const char DeviceCapabilities::kKeyDisplaySupported[] = "display_supported";
const char DeviceCapabilities::kKeyHiResAudioSupported[] =
    "hi_res_audio_supported";

// static
std::unique_ptr<DeviceCapabilities> DeviceCapabilities::Create() {
  return base::WrapUnique(new DeviceCapabilitiesImpl);
}

// static
std::unique_ptr<DeviceCapabilities> DeviceCapabilities::CreateForTesting() {
  DeviceCapabilities* capabilities = new DeviceCapabilitiesImpl;
  capabilities->SetCapability(kKeyBluetoothSupported,
                              std::make_unique<base::Value>(false));
  capabilities->SetCapability(kKeyDisplaySupported,
                              std::make_unique<base::Value>(true));
  capabilities->SetCapability(kKeyHiResAudioSupported,
                              std::make_unique<base::Value>(false));
  capabilities->SetCapability(kKeyAssistantSupported,
                              std::make_unique<base::Value>(true));
  return base::WrapUnique(capabilities);
}

scoped_refptr<DeviceCapabilities::Data> DeviceCapabilities::CreateData() {
  return base::WrapRefCounted(new Data);
}

scoped_refptr<DeviceCapabilities::Data> DeviceCapabilities::CreateData(
    std::unique_ptr<const base::DictionaryValue> dictionary) {
  DCHECK(dictionary.get());
  return base::WrapRefCounted(new Data(std::move(dictionary)));
}

DeviceCapabilities::Validator::Validator(DeviceCapabilities* capabilities)
    : capabilities_(capabilities) {
  DCHECK(capabilities);
}

void DeviceCapabilities::Validator::SetPublicValidatedValue(
    const std::string& path,
    std::unique_ptr<base::Value> new_value) const {
  capabilities_->SetPublicValidatedValue(path, std::move(new_value));
}

void DeviceCapabilities::Validator::SetPrivateValidatedValue(
    const std::string& path,
    std::unique_ptr<base::Value> new_value) const {
  capabilities_->SetPrivateValidatedValue(path, std::move(new_value));
}

DeviceCapabilities::Data::Data()
    : dictionary_(new base::DictionaryValue),
      json_string_(*SerializeToJson(*dictionary_)) {}

DeviceCapabilities::Data::Data(
    std::unique_ptr<const base::DictionaryValue> dictionary)
    : dictionary_(std::move(dictionary)),
      json_string_(*SerializeToJson(*dictionary_)) {
  DCHECK(dictionary_.get());
}

DeviceCapabilitiesImpl::Data::~Data() {}

DeviceCapabilitiesImpl::ValidatorInfo::ValidatorInfo(Validator* validator)
    : validator_(validator), task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(validator_);
  DCHECK(task_runner_.get());
}

DeviceCapabilitiesImpl::ValidatorInfo::~ValidatorInfo() {
  // Check that ValidatorInfo is being destroyed on the same thread that it was
  // constructed on.
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void DeviceCapabilitiesImpl::ValidatorInfo::Validate(
    const std::string& path,
    std::unique_ptr<base::Value> proposed_value) const {
  // Check that we are running Validate on the same thread that ValidatorInfo
  // was constructed on.
  DCHECK(task_runner_->BelongsToCurrentThread());
  validator_->Validate(path, std::move(proposed_value));
}

DeviceCapabilitiesImpl::DeviceCapabilitiesImpl()
    : all_data_(CreateData()),
      public_data_(CreateData()),
      task_runner_for_writes_(base::ThreadTaskRunnerHandle::Get()),
      observer_list_(new base::ObserverListThreadSafe<Observer>) {
  DCHECK(task_runner_for_writes_.get());
}

DeviceCapabilitiesImpl::~DeviceCapabilitiesImpl() {
  // Make sure that any registered Validators have unregistered at this point
  DCHECK(validator_map_.empty())
      << "Some validators weren't properly unregistered: " << [this] {
           std::vector<std::string> keys;
           for (const auto& pair : validator_map_) {
             keys.push_back(pair.first);
           }
           return base::JoinString(keys, ", ");
         }();
  // Make sure that all observers have been removed at this point
  observer_list_->AssertEmpty();
}

void DeviceCapabilitiesImpl::Register(const std::string& key,
                                      Validator* validator) {
  DCHECK(IsValidRegisterKey(key));
  DCHECK(validator);

  base::AutoLock auto_lock(validation_lock_);
  // Check that a validator has not already been registered for this key
  DCHECK_EQ(0u, validator_map_.count(key));
  validator_map_[key] = std::make_unique<ValidatorInfo>(validator);
}

void DeviceCapabilitiesImpl::Unregister(const std::string& key,
                                        const Validator* validator) {
  base::AutoLock auto_lock(validation_lock_);
  auto validator_it = validator_map_.find(key);
  DCHECK(validator_it != validator_map_.end());
  // Check that validator being unregistered matches the original for |key|.
  // This prevents managers from accidentally unregistering incorrect
  // validators.
  DCHECK_EQ(validator, validator_it->second->validator());
  // Check that validator is unregistering on same thread that it was
  // registered on
  DCHECK(validator_it->second->task_runner()->BelongsToCurrentThread());
  validator_map_.erase(validator_it);
}

DeviceCapabilities::Validator* DeviceCapabilitiesImpl::GetValidator(
    const std::string& key) const {
  base::AutoLock auto_lock(validation_lock_);
  auto validator_it = validator_map_.find(key);
  return validator_it == validator_map_.end()
             ? nullptr
             : validator_it->second->validator();
}

bool DeviceCapabilitiesImpl::BluetoothSupported() const {
  scoped_refptr<Data> data_ref = GetAllData();
  bool bluetooth_supported = false;
  bool found_key = data_ref->dictionary().GetBoolean(kKeyBluetoothSupported,
                                                     &bluetooth_supported);
  DCHECK(found_key);
  return bluetooth_supported;
}

bool DeviceCapabilitiesImpl::DisplaySupported() const {
  scoped_refptr<Data> data_ref = GetAllData();
  bool display_supported = false;
  bool found_key = data_ref->dictionary().GetBoolean(kKeyDisplaySupported,
                                                     &display_supported);
  DCHECK(found_key);
  return display_supported;
}

bool DeviceCapabilitiesImpl::HiResAudioSupported() const {
  scoped_refptr<Data> data_ref = GetAllData();
  bool hi_res_audio_supported = false;
  bool found_key = data_ref->dictionary().GetBoolean(kKeyHiResAudioSupported,
                                                     &hi_res_audio_supported);
  DCHECK(found_key);
  return hi_res_audio_supported;
}

bool DeviceCapabilitiesImpl::AssistantSupported() const {
  scoped_refptr<Data> data_ref = GetAllData();
  bool assistant_supported = false;
  bool found_key = data_ref->dictionary().GetBoolean(kKeyAssistantSupported,
                                                     &assistant_supported);
  DCHECK(found_key);
  return assistant_supported;
}

std::unique_ptr<base::Value> DeviceCapabilitiesImpl::GetCapability(
    const std::string& path) const {
  scoped_refptr<Data> data_ref = GetAllData();
  const base::Value* value = nullptr;
  bool found_path = data_ref->dictionary().Get(path, &value);
  return found_path ? value->CreateDeepCopy() : std::unique_ptr<base::Value>();
}

scoped_refptr<DeviceCapabilities::Data> DeviceCapabilitiesImpl::GetAllData()
    const {
  // Need to acquire lock here when copy constructing all_data_ otherwise we
  // could concurrently be writing to scoped_refptr in SetPublicValidatedValue()
  // or SetPrivateValidatedValue(), which could cause a bad scoped_refptr read.
  base::AutoLock auto_lock(data_lock_);
  return all_data_;
}

scoped_refptr<DeviceCapabilities::Data> DeviceCapabilitiesImpl::GetPublicData()
    const {
  // Need to acquire lock here when copy constructing public_data_ otherwise we
  // could concurrently be writing to scoped_refptr in SetPublicValidatedValue()
  // or SetPrivateValidatedValue(), which could cause a bad scoped_refptr read.
  base::AutoLock auto_lock(data_lock_);
  return public_data_;
}

void DeviceCapabilitiesImpl::SetCapability(
    const std::string& path,
    std::unique_ptr<base::Value> proposed_value) {
  DCHECK(proposed_value.get());
  if (!IsValidPath(path)) {
    LOG(DFATAL) << "Invalid capability path encountered for SetCapability()";
    return;
  }

  {
    base::AutoLock auto_lock(validation_lock_);
    // Check for Validator registered under first key per the Register()
    // interface.
    auto validator_it = validator_map_.find(GetFirstKey(path));
    if (validator_it != validator_map_.end()) {
      // We do not want to post a task directly for the Validator's Validate()
      // method here because if another thread is in the middle of unregistering
      // that Validator, there will be an outstanding call to Validate() that
      // occurs after it has unregistered. Since ValidatorInfo gets destroyed
      // in Unregister() on same thread that validation should run on, we can
      // post a task to the Validator's thread with weak_ptr. This way, if the
      // Validator gets unregistered, the call to Validate will get skipped.
      validator_it->second->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&ValidatorInfo::Validate,
                                    validator_it->second->AsWeakPtr(), path,
                                    std::move(proposed_value)));
      return;
    }
  }
  // Since we are done checking for a registered Validator at this point, we
  // can release the lock. All further member access will be for capabilities.
  // By default, a capability without a validator will be public.
  SetPublicValidatedValue(path, std::move(proposed_value));
}

void DeviceCapabilitiesImpl::MergeDictionary(
    const base::DictionaryValue& dict_value) {
  for (base::DictionaryValue::Iterator it(dict_value); !it.IsAtEnd();
       it.Advance()) {
    SetCapability(it.key(), it.value().CreateDeepCopy());
  }
}

void DeviceCapabilitiesImpl::AddCapabilitiesObserver(Observer* observer) {
  DCHECK(observer);
  observer_list_->AddObserver(observer);
}

void DeviceCapabilitiesImpl::RemoveCapabilitiesObserver(Observer* observer) {
  DCHECK(observer);
  observer_list_->RemoveObserver(observer);
}

void DeviceCapabilitiesImpl::SetPublicValidatedValue(
    const std::string& path,
    std::unique_ptr<base::Value> new_value) {
  // All internal writes/modifications of capabilities must occur on same
  // thread to avoid race conditions.
  if (!task_runner_for_writes_->BelongsToCurrentThread()) {
    task_runner_for_writes_->PostTask(
        FROM_HERE,
        base::BindOnce(&DeviceCapabilitiesImpl::SetPublicValidatedValue,
                       base::Unretained(this), path, std::move(new_value)));
    return;
  }

  DCHECK(IsValidPath(path));
  DCHECK(new_value.get());

  // If the capability exists, it must be public (present in all_data_ and
  // public_data_). We cannot change the privacy of an already existing
  // capability.
  bool is_private = all_data_->dictionary().HasKey(path) &&
                    !public_data_->dictionary().HasKey(path);
  if (is_private) {
    NOTREACHED() << "Cannot make a private capability '" << path << "' public.";
    return;
  }

  // We don't need to acquire lock here when reading public_data_ because we
  // know that all writes to public_data_ must occur serially on thread that
  // we're on.
  const base::Value* cur_value = nullptr;
  bool capability_unchanged =
      public_data_->dictionary().Get(path, &cur_value) &&
      cur_value->Equals(new_value.get());
  if (capability_unchanged) {
    DVLOG(1) << "Ignoring unchanged public capability: " << path;
    return;
  }

  // In this sequence, we create deep copies for both dictionaries, modify the
  // copies, and then do a pointer swap. We do this to have minimal time spent
  // in the data_lock_. If we were to lock and modify the capabilities
  // dictionary directly, there may be expensive writes that block other
  // threads.
  scoped_refptr<Data> new_public_data = GenerateDataWithNewValue(
      public_data_->dictionary(), path, new_value->CreateDeepCopy());
  scoped_refptr<Data> new_data = GenerateDataWithNewValue(
      all_data_->dictionary(), path, std::move(new_value));

  {
    base::AutoLock auto_lock(data_lock_);
    // Using swap instead of assignment operator here because it's a little
    // faster. Avoids an extra call to AddRef()/Release().
    public_data_.swap(new_public_data);
    all_data_.swap(new_data);
  }

  // Even though ObserverListThreadSafe notifications are always asynchronous
  // (posts task even if to same thread), no locks should be held at this point
  // in the code. This is just to be safe that no deadlocks occur if Observers
  // call DeviceCapabilities methods in OnCapabilitiesChanged().
  observer_list_->Notify(FROM_HERE, &Observer::OnCapabilitiesChanged, path);
}

void DeviceCapabilitiesImpl::SetPrivateValidatedValue(
    const std::string& path,
    std::unique_ptr<base::Value> new_value) {
  // All internal writes/modifications of capabilities must occur on same
  // thread to avoid race conditions.
  if (!task_runner_for_writes_->BelongsToCurrentThread()) {
    task_runner_for_writes_->PostTask(
        FROM_HERE,
        base::BindOnce(&DeviceCapabilitiesImpl::SetPrivateValidatedValue,
                       base::Unretained(this), path, std::move(new_value)));
    return;
  }

  DCHECK(IsValidPath(path));
  DCHECK(new_value.get());

  // If the capability exists, it must be private (present in all_data_ only).
  // We cannot change the privacy of an already existing capability.
  bool is_public = public_data_->dictionary().HasKey(path);
  if (is_public) {
    NOTREACHED() << "Cannot make a public capability '" << path << "' private.";
    return;
  }

  // We don't need to acquire lock here when reading all_data_ because we know
  // that all writes to all_data_ must occur serially on thread that we're on.
  const base::Value* cur_value = nullptr;
  bool capability_unchanged = all_data_->dictionary().Get(path, &cur_value) &&
                              cur_value->Equals(new_value.get());
  if (capability_unchanged) {
    DVLOG(1) << "Ignoring unchanged capability: " << path;
    return;
  }

  // In this sequence, we create a deep copy, modify the deep copy, and then
  // do a pointer swap. We do this to have minimal time spent in the
  // data_lock_. If we were to lock and modify the capabilities
  // dictionary directly, there may be expensive writes that block other
  // threads.
  scoped_refptr<Data> new_data = GenerateDataWithNewValue(
      all_data_->dictionary(), path, std::move(new_value));

  {
    base::AutoLock auto_lock(data_lock_);
    // Using swap instead of assignment operator here because it's a little
    // faster. Avoids an extra call to AddRef()/Release().
    all_data_.swap(new_data);
  }

  // Even though ObserverListThreadSafe notifications are always asynchronous
  // (posts task even if to same thread), no locks should be held at this point
  // in the code. This is just to be safe that no deadlocks occur if Observers
  // call DeviceCapabilities methods in OnCapabilitiesChanged().
  observer_list_->Notify(FROM_HERE, &Observer::OnCapabilitiesChanged, path);
}

scoped_refptr<DeviceCapabilities::Data>
DeviceCapabilitiesImpl::GenerateDataWithNewValue(
    const base::DictionaryValue& dict,
    const std::string& path,
    std::unique_ptr<base::Value> new_value) {
  std::unique_ptr<base::DictionaryValue> dict_deep_copy(dict.CreateDeepCopy());
  dict_deep_copy->Set(path, std::move(new_value));
  return CreateData(std::move(dict_deep_copy));
}

}  // namespace chromecast
