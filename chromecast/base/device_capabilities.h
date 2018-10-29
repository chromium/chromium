// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_DEVICE_CAPABILITIES_H_
#define CHROMECAST_BASE_DEVICE_CAPABILITIES_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace chromecast {

// Device capabilities are a set of features used to determine what operations
// are available on the device. They are identified by a key (string) and a
// value (base::Value). The class serves 2 main purposes:
//
// 1) Provide an interface for updating default capabilities and querying their
// current value. Default capabilities are known to the system beforehand
// and used by modules throughout Chromecast to control behavior of operations.
//
// 2) Store dynamic capabilities. Dynamic capabilities are not known to the
// system beforehand and are introduced by external parties. These capabilites
// are stored and then forwarded to app servers that use them to determine how
// to interact with the device.
//
// Capabilities can be classified as either "public" or "private". Capabilities
// of both types can be used by the Chromecast platform to control internal
// behaviors, but only public capabilities will be advertised to app servers.
// Once a capability is set, it retains its privacy classification permanently;
// attempting to change the privacy of a capability results in an error.
// Private capabilities can only be added by Validators. Calling SetCapability()
// on a path without a Validator will default to setting the capability as
// public.
//
// Thread Safety:
// Observers can be added from any thread. Each Observer is guaranteed to be
// notified on same thread that it was added on and must be removed on the same
// thread that it was added on.
//
// Validators can be registered from any thread. Each Validator's Validate()
// method is guaranteed to be called on same thread that the Validator was
// registered on. The Validator must be unregistered on the same thread
// that it was registered on.
//
// All other methods can be called safely from any thread.

// TODO(esum):
// 1) Add WifiSupported, HotspotSupported, and MultizoneSupported capabilities.
// 2) It's not ideal to have the accessors (BluetoothSupported(), etc.) not
//    be valid initially until the capability gets registered. We might want
//    to use some kind of builder class to solve this.
class DeviceCapabilities {
 public:
  class Observer {
   public:
    // Called when DeviceCapabilities gets written to in any way. |path|
    // is full path to capability that has been updated.
    virtual void OnCapabilitiesChanged(const std::string& path) = 0;

   protected:
    virtual ~Observer() {}
  };

  // When another module attempts to update the value for a capability,
  // a manager may want to validate the change or even modify the new value.
  // Managers that wish to perform this validation should inherit from the
  // Validator class and implement its interface.
  class Validator {
   public:
    // |path| is full path to capability, which could include paths expanded on
    // the capability key that gets registered through the Register() method.
    // For example, if a key of "foo" is registered for a Validator, |path|
    // could be "foo", "foo.bar", "foo.bar.what", etc. |proposed_value| is new
    // value being proposed for |path|. Determines if |proposed_value| is valid
    // change for |path|. This method may be asynchronous, but multiple calls
    // to it must be handled serially. Returns response through
    // SetPublicValidatedValue() or SetPrivateValidatedValue().
    virtual void Validate(const std::string& path,
                          std::unique_ptr<base::Value> proposed_value) = 0;

   protected:
    explicit Validator(DeviceCapabilities* capabilities);
    virtual ~Validator() {}

    DeviceCapabilities* capabilities() const { return capabilities_; }

    // Meant to be called when Validate() has finished. |path| is full path to
    // capability. |new_value| is new validated value to be used in
    // DeviceCapabilities. This method passes these parameters to
    // DeviceCapabilities, where |path| is updated internally to |new_value|.
    // TODO(seantopping): Change this interface so that Validators are not the
    // only means of accessing private capabilities.
    void SetPublicValidatedValue(const std::string& path,
                                 std::unique_ptr<base::Value> new_value) const;
    void SetPrivateValidatedValue(const std::string& path,
                                  std::unique_ptr<base::Value> new_value) const;

   private:
    DeviceCapabilities* const capabilities_;

    DISALLOW_COPY_AND_ASSIGN(Validator);
  };

  // Class used to store/own capabilities-related data. It is immutable and
  // RefCountedThreadSafe, so client code can freely query it throughout its
  // lifetime without worrying about the data getting invalidated in any way.
  class Data : public base::RefCountedThreadSafe<Data> {
   public:
    // Accessor for complete capabilities in dictionary format.
    const base::DictionaryValue& dictionary() const {
      return *dictionary_.get();
    }

    // Accessor for complete capabilities string in JSON format.
    const std::string& json_string() const { return json_string_; }

   private:
    friend class base::RefCountedThreadSafe<Data>;
    // DeviceCapabilities should be the only one responsible for Data
    // construction. See CreateData() methods.
    friend class DeviceCapabilities;

    // Constructs empty dictionary with no capabilities.
    Data();
    // Uses |dictionary| as capabilities dictionary.
    explicit Data(std::unique_ptr<const base::DictionaryValue> dictionary);
    ~Data();

    const std::unique_ptr<const base::DictionaryValue> dictionary_;
    const std::string json_string_;

    DISALLOW_COPY_AND_ASSIGN(Data);
  };

  // Default Capability keys
  static const char kKeyAssistantSupported[];
  static const char kKeyBluetoothSupported[];
  static const char kKeyDisplaySupported[];
  static const char kKeyHiResAudioSupported[];

  // This class should get destroyed after all Validators have been
  // unregistered, all Observers have been removed, and the class is no longer
  // being accessed.
  virtual ~DeviceCapabilities() {}

  // Create empty instance with no capabilities. Although the class is not
  // singleton, there is meant to be a single instance owned by another module.
  // The instance should be created early enough for all managers to register
  // themselves, and then live long enough for all managers to unregister.
  static std::unique_ptr<DeviceCapabilities> Create();
  // Creates an instance where all the default capabilities are initialized
  // to a predefined default value, and no Validators are registered. For use
  // only in unit tests.
  static std::unique_ptr<DeviceCapabilities> CreateForTesting();

  // Registers a Validator for a capability. A given key must only be
  // registered once, and must be unregistered before calling Register() again.
  // If the capability has a value of Dictionary type, |key| must be just
  // the capability's top-level key and not include path expansions to levels
  // farther down. For example, "foo" is a valid value for |key|, but "foo.bar"
  // is not. Note that if "foo.bar" is updated in SetCapability(), the
  // Validate() method for "foo"'s Validator will be called, with a |path| of
  // "foo.bar". Note that this method does not add or modify the capability.
  // To do this, SetCapability() should be called, or Validators can call
  // SetPublicValidatedValue() or SetPrivateValidatedValue(). This method is
  // synchronous to ensure Validators know exactly when they may start receiving
  // validation requests.
  virtual void Register(const std::string& key,
                        Validator* validator) = 0;
  // Unregisters Validator for |key|. |validator| argument must match
  // |validator| argument that was passed in to Register() for |key|. Note that
  // the capability and its value remain untouched. This method is synchronous
  // to ensure Validators know exactly when they will stop receiving validation
  // requests.
  virtual void Unregister(const std::string& key,
                          const Validator* validator) = 0;
  // Gets the Validator currently registered for |key|. Returns nullptr if
  // no Validator is registered.
  virtual Validator* GetValidator(const std::string& key) const = 0;

  // Accessors for default capabilities. Note that the capability must be added
  // through SetCapability() (or Set[Private]ValidatedValue() for Validators)
  // before accessors are called.
  virtual bool AssistantSupported() const = 0;
  virtual bool BluetoothSupported() const = 0;
  virtual bool DisplaySupported() const = 0;
  virtual bool HiResAudioSupported() const = 0;

  // Returns a deep copy of the value at |path|. If the capability at |path|
  // does not exist, a null scoped_ptr is returned.
  virtual std::unique_ptr<base::Value> GetCapability(
      const std::string& path) const = 0;

  // Use this method to access dictionary and JSON string. No deep copying is
  // performed, so this method is inexpensive. Note that any capability updates
  // that occur after GetAllData() has been called will not be reflected in the
  // returned scoped_refptr. You can think of this method as taking a snapshot
  // of the capabilities when it gets called. All capabilities (those set by
  // SetPrivateValidatedValue() and SetPublicValidatedValue()) will be present
  // in the returned Data object.
  virtual scoped_refptr<Data> GetAllData() const = 0;
  // Similar to GetAllData(), but this only returns public capabilities.
  virtual scoped_refptr<Data> GetPublicData() const = 0;

  // Updates the value at |path| to |proposed_value| if |path| already exists
  // and adds new capability if |path| doesn't. Note that if a key has been
  // registered that is at the beginning of |path|, then the Validator will be
  // used to determine if |proposed_value| is accepted.
  // Ex: If "foo" has a Validator registered, a |path| of "foo.bar"
  // will cause |proposed_value| to go through the Validator's Validate()
  // method. Client code may use the Observer interface to determine the
  // ultimate value used.
  // This method is asynchronous. By default, this method will classify the new
  // value at |path| as a public capability; if a Validator is present, it may
  // classify the value as public or private via SetPublicValidatedValue() or
  // SetPrivateValidatedValue() respectively.
  virtual void SetCapability(const std::string& path,
                             std::unique_ptr<base::Value> proposed_value) = 0;

  // Iterates through entries in |dict_value| and calls SetCapability() for
  // each one. This method is asynchronous.
  virtual void MergeDictionary(const base::DictionaryValue& dict_value) = 0;

  // Adds/removes an observer. It doesn't take the ownership of |observer|.
  virtual void AddCapabilitiesObserver(Observer* observer) = 0;
  virtual void RemoveCapabilitiesObserver(Observer* observer) = 0;

 protected:
  DeviceCapabilities() {}

  // For derived implementation classes to create Data instances since they do
  // not have access to Data constructors.
  // Creates empty dictionary with no capabilities.
  static scoped_refptr<Data> CreateData();
  // Uses |dictionary| as capabilities dictionary.
  static scoped_refptr<Data> CreateData(
      std::unique_ptr<const base::DictionaryValue> dictionary);

 private:
  // Internally update the capability residing at |path| to |new_value|. This
  // capability will be visible in GetAllData() and GetPublicData().
  virtual void SetPublicValidatedValue(
      const std::string& path,
      std::unique_ptr<base::Value> new_value) = 0;
  // Similar to SetPublicValidatedValue(), but this capability will only be
  // visible in GetAllData().
  virtual void SetPrivateValidatedValue(
      const std::string& path,
      std::unique_ptr<base::Value> new_value) = 0;

  DISALLOW_COPY_AND_ASSIGN(DeviceCapabilities);
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_DEVICE_CAPABILITIES_H_
