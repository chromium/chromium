// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_DEVICE_CAPABILITIES_IMPL_H_
#define CHROMECAST_BASE_DEVICE_CAPABILITIES_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chromecast/base/device_capabilities.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace chromecast {

class DeviceCapabilitiesImpl : public DeviceCapabilities {
 public:
  DeviceCapabilitiesImpl(const DeviceCapabilitiesImpl&) = delete;
  DeviceCapabilitiesImpl& operator=(const DeviceCapabilitiesImpl&) = delete;

  ~DeviceCapabilitiesImpl() override;

  // DeviceCapabilities implementation:
  void Register(const std::string& key, Validator* validator) override;
  void Unregister(const std::string& key, const Validator* validator) override;
  Validator* GetValidator(const std::string& key) const override;
  bool AssistantSupported() const override;
  bool BluetoothSupported() const override;
  bool DisplaySupported() const override;
  bool HiResAudioSupported() const override;
  base::Value GetCapability(const std::string& path) const override;
  scoped_refptr<Data> GetAllData() const override;
  scoped_refptr<Data> GetPublicData() const override;
  void SetCapability(const std::string& path,
                     base::Value proposed_value) override;
  void MergeDictionary(const base::Value::Dict& dict) override;
  void AddCapabilitiesObserver(Observer* observer) override;
  void RemoveCapabilitiesObserver(Observer* observer) override;

 private:
  class ValidatorInfo final {
   public:
    explicit ValidatorInfo(Validator* validator);

    ValidatorInfo(const ValidatorInfo&) = delete;
    ValidatorInfo& operator=(const ValidatorInfo&) = delete;

    ~ValidatorInfo();

    Validator* validator() const { return validator_; }

    scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
      return task_runner_;
    }

    void Validate(const std::string& path, base::Value proposed_value) const;

    base::WeakPtr<ValidatorInfo> AsWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

   private:
    Validator* const validator_;
    // TaskRunner of thread that validator_ was registered on
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

    base::WeakPtrFactory<ValidatorInfo> weak_ptr_factory_{this};
  };

  // For DeviceCapabilitiesImpl()
  friend class DeviceCapabilities;

  // Map from capability key to corresponding ValidatorInfo. Gets updated
  // in Register()/Unregister().
  using ValidatorMap =
      std::unordered_map<std::string, std::unique_ptr<ValidatorInfo>>;

  // Internal constructor used by static DeviceCapabilities::Create*() methods.
  DeviceCapabilitiesImpl();

  void SetPublicValidatedValue(const std::string& path,
                               base::Value new_value) override;
  void SetPrivateValidatedValue(const std::string& path,
                                base::Value new_value) override;
  void SetValidatedValueInternal(const std::string& path,
                                 base::Value new_value);

  scoped_refptr<Data> GenerateDataWithNewValue(const base::Value::Dict& dict,
                                               const std::string& path,
                                               base::Value new_value);

  // Lock for reading/writing all_data_ or public_data_ pointers
  mutable base::Lock data_lock_;
  // Lock for reading/writing validator_map_
  mutable base::Lock validation_lock_;

  // Contains all public and private capabilities.
  scoped_refptr<Data> all_data_;
  // Contains only public capabilities. All capabilities in public_data_ are
  // present and duplicated in all_data_. This duplication allows callers to
  // quickly query public capabilities without having build a new data
  // dictionary.
  scoped_refptr<Data> public_data_;
  // TaskRunner for capability writes. All internal writes to data_ must occur
  // on task_runner_for_writes_'s thread.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_writes_;

  ValidatorMap validator_map_;
  const scoped_refptr<base::ObserverListThreadSafe<Observer>> observer_list_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_DEVICE_CAPABILITIES_IMPL_H_
