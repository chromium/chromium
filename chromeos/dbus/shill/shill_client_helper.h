// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_SHILL_CLIENT_HELPER_H_
#define CHROMEOS_DBUS_SHILL_SHILL_CLIENT_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/shill/shill_property_changed_observer.h"

namespace base {

class Value;
class DictionaryValue;

}  // namespace base

namespace dbus {

class MessageWriter;
class MethodCall;
class ObjectProxy;
class Signal;

}  // namespace dbus

namespace chromeos {

// A class to help implement Shill clients.
class ShillClientHelper {
 public:
  class RefHolder;

  // A callback to handle responses for methods with Value of type DICTIONARY
  // results.
  // TODO(crbug.com/1109627): Consider renaming this since it is no longer a
  // DictionaryValue.
  using DictionaryValueCallback = DBusMethodCallback<base::Value>;

  // A callback to handle responses for methods with Value of type DICTIONARY
  // results. This is used by CallDictionaryValueMethodWithErrorCallback.
  using DictionaryValueCallbackWithoutStatus =
      base::OnceCallback<void(base::Value result)>;

  // A callback to handle responses of methods returning a ListValue.
  using ListValueCallback =
      base::OnceCallback<void(const base::ListValue& result)>;

  // A callback to handle errors for method call.
  using ErrorCallback =
      base::OnceCallback<void(const std::string& error_name,
                              const std::string& error_message)>;

  // A callback that handles responses for methods with string results.
  using StringCallback = base::OnceCallback<void(const std::string& result)>;

  // A callback that handles responses for methods with boolean results.
  using BooleanCallback = base::OnceCallback<void(bool result)>;

  // Callback used to notify owner when this can be safely released.
  using ReleasedCallback = base::OnceCallback<void(ShillClientHelper* helper)>;

  explicit ShillClientHelper(dbus::ObjectProxy* proxy);

  virtual ~ShillClientHelper();

  // Sets |released_callback_|. This is optional and should only be called at
  // most once.
  void SetReleasedCallback(ReleasedCallback callback);

  // Adds an |observer| of the PropertyChanged signal.
  void AddPropertyChangedObserver(ShillPropertyChangedObserver* observer);

  // Removes an |observer| of the PropertyChanged signal.
  void RemovePropertyChangedObserver(ShillPropertyChangedObserver* observer);

  // Starts monitoring PropertyChanged signal. If there aren't observers for the
  // PropertyChanged signal, the actual monitoring will be delayed until the
  // first observer is added.
  void MonitorPropertyChanged(const std::string& interface_name);

  // Calls a method without results.
  void CallVoidMethod(dbus::MethodCall* method_call,
                      VoidDBusMethodCallback callback);

  // Calls a method with an object path result where there is an error callback.
  void CallObjectPathMethodWithErrorCallback(dbus::MethodCall* method_call,
                                             ObjectPathCallback callback,
                                             ErrorCallback error_callback);

  // Calls a method with a dictionary value result.
  void CallDictionaryValueMethod(dbus::MethodCall* method_call,
                                 DictionaryValueCallback callback);

  // Calls a method without results with error callback.
  void CallVoidMethodWithErrorCallback(dbus::MethodCall* method_call,
                                       base::OnceClosure callback,
                                       ErrorCallback error_callback);

  // Calls a method with a boolean result with error callback.
  void CallBooleanMethodWithErrorCallback(dbus::MethodCall* method_call,
                                          BooleanCallback callback,
                                          ErrorCallback error_callback);

  // Calls a method with a string result with error callback.
  void CallStringMethodWithErrorCallback(dbus::MethodCall* method_call,
                                         StringCallback callback,
                                         ErrorCallback error_callback);

  // Calls a method with a dictionary value result with error callback.
  void CallDictionaryValueMethodWithErrorCallback(
      dbus::MethodCall* method_call,
      DictionaryValueCallbackWithoutStatus callback,
      ErrorCallback error_callback);

  // Calls a method with a boolean array result with error callback.
  void CallListValueMethodWithErrorCallback(dbus::MethodCall* method_call,
                                            ListValueCallback callback,
                                            ErrorCallback error_callback);

  const dbus::ObjectProxy* object_proxy() const { return proxy_; }

  // Appends the value to the writer as a variant. If |value| is a Dictionary it
  // will be written as a string -> varient dictionary, a{sv}. If |value| is a
  // List then it must be a List of String values and is writen as type 'as'.
  static void AppendValueDataAsVariant(dbus::MessageWriter* writer,
                                       const base::Value& value);

  // Appends a string-to-variant dictionary to the writer as an '{sv}' array.
  // Each value is written using AppendValueDataAsVariant.
  static void AppendServicePropertiesDictionary(dbus::MessageWriter* writer,
                                                const base::Value& dictionary);

 protected:
  // Reference / Ownership management. If the number of active refs (observers
  // + in-progress method calls) becomes 0, |released_callback_| (if set) will
  // be called.
  void AddRef();
  void Release();

 private:
  // Starts monitoring PropertyChanged signal.
  void MonitorPropertyChangedInternal(const std::string& interface_name);

  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool success);

  // Handles PropertyChanged signal.
  void OnPropertyChanged(dbus::Signal* signal);

  dbus::ObjectProxy* proxy_;
  ReleasedCallback released_callback_;
  int active_refs_;
  base::ObserverList<ShillPropertyChangedObserver,
                     true /* check_empty */>::Unchecked observer_list_;
  std::vector<std::string> interfaces_to_be_monitored_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShillClientHelper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ShillClientHelper);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_SHILL_CLIENT_HELPER_H_
