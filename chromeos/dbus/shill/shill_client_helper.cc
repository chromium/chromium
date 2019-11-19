// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill/shill_client_helper.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// Class to hold onto a reference to a ShillClientHelper. This calss
// is owned by callbacks and released once the callback completes.
// Note: Only success callbacks hold the reference. If an error callback is
// invoked instead, the success callback will still be destroyed and the
// RefHolder with it, once the callback chain completes.
class ShillClientHelper::RefHolder {
 public:
  explicit RefHolder(base::WeakPtr<ShillClientHelper> helper)
      : helper_(helper),
        origin_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
    helper_->AddRef();
  }
  ~RefHolder() {
    // Release the helper on the origin thread.
    base::Closure closure = base::Bind(&ShillClientHelper::Release, helper_);
    if (origin_task_runner_->BelongsToCurrentThread()) {
      closure.Run();
    } else {
      origin_task_runner_->PostTask(FROM_HERE, closure);
    }
  }

 private:
  base::WeakPtr<ShillClientHelper> helper_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;
};

namespace {

const char kInvalidResponseErrorName[] = "";  // No error name.
const char kInvalidResponseErrorMessage[] = "Invalid response.";

// Note: here and below, |ref_holder| is unused in the function body. It only
// exists so that it will be destroyed (and the reference released) with the
// Callback object once completed.
void OnBooleanMethodWithErrorCallback(
    ShillClientHelper::RefHolder* ref_holder,
    const ShillClientHelper::BooleanCallback& callback,
    const ShillClientHelper::ErrorCallback& error_callback,
    dbus::Response* response) {
  if (!response) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  dbus::MessageReader reader(response);
  bool result;
  if (!reader.PopBool(&result)) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  callback.Run(result);
}

void OnStringMethodWithErrorCallback(
    ShillClientHelper::RefHolder* ref_holder,
    const ShillClientHelper::StringCallback& callback,
    const ShillClientHelper::ErrorCallback& error_callback,
    dbus::Response* response) {
  if (!response) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  dbus::MessageReader reader(response);
  std::string result;
  if (!reader.PopString(&result)) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  callback.Run(result);
}

// Handles responses for methods without results.
void OnVoidMethod(ShillClientHelper::RefHolder* ref_holder,
                  VoidDBusMethodCallback callback,
                  dbus::Response* response) {
  std::move(callback).Run(response != nullptr);
}

// Handles responses for methods with ObjectPath results and no status.
void OnObjectPathMethodWithoutStatus(
    ShillClientHelper::RefHolder* ref_holder,
    const ObjectPathCallback& callback,
    const ShillClientHelper::ErrorCallback& error_callback,
    dbus::Response* response) {
  if (!response) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  dbus::MessageReader reader(response);
  dbus::ObjectPath result;
  if (!reader.PopObjectPath(&result)) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  callback.Run(result);
}

// Handles responses for methods with DictionaryValue results.
void OnDictionaryValueMethod(
    ShillClientHelper::RefHolder* ref_holder,
    const ShillClientHelper::DictionaryValueCallback& callback,
    dbus::Response* response) {
  if (!response) {
    base::DictionaryValue result;
    callback.Run(DBUS_METHOD_CALL_FAILURE, result);
    return;
  }
  dbus::MessageReader reader(response);
  std::unique_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  base::DictionaryValue* result = NULL;
  if (!value.get() || !value->GetAsDictionary(&result)) {
    base::DictionaryValue result;
    callback.Run(DBUS_METHOD_CALL_FAILURE, result);
    return;
  }
  callback.Run(DBUS_METHOD_CALL_SUCCESS, *result);
}

// Handles responses for methods without results.
void OnVoidMethodWithErrorCallback(ShillClientHelper::RefHolder* ref_holder,
                                   const base::Closure& callback,
                                   dbus::Response* response) {
  callback.Run();
}

// Handles responses for methods with DictionaryValue results.
// Used by CallDictionaryValueMethodWithErrorCallback().
void OnDictionaryValueMethodWithErrorCallback(
    ShillClientHelper::RefHolder* ref_holder,
    const ShillClientHelper::DictionaryValueCallbackWithoutStatus& callback,
    const ShillClientHelper::ErrorCallback& error_callback,
    dbus::Response* response) {
  dbus::MessageReader reader(response);
  std::unique_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  base::DictionaryValue* result = NULL;
  if (!value.get() || !value->GetAsDictionary(&result)) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  callback.Run(*result);
}

// Handles responses for methods with ListValue results.
void OnListValueMethodWithErrorCallback(
    ShillClientHelper::RefHolder* ref_holder,
    const ShillClientHelper::ListValueCallback& callback,
    const ShillClientHelper::ErrorCallback& error_callback,
    dbus::Response* response) {
  dbus::MessageReader reader(response);
  std::unique_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  base::ListValue* result = NULL;
  if (!value.get() || !value->GetAsList(&result)) {
    error_callback.Run(kInvalidResponseErrorName, kInvalidResponseErrorMessage);
    return;
  }
  callback.Run(*result);
}

// Handles running appropriate error callbacks.
void OnError(const ShillClientHelper::ErrorCallback& error_callback,
             dbus::ErrorResponse* response) {
  std::string error_name;
  std::string error_message;
  if (response) {
    // Error message may contain the error message as string.
    dbus::MessageReader reader(response);
    error_name = response->GetErrorName();
    reader.PopString(&error_message);
  }
  error_callback.Run(error_name, error_message);
}

}  // namespace

ShillClientHelper::ShillClientHelper(dbus::ObjectProxy* proxy)
    : proxy_(proxy), active_refs_(0) {}

ShillClientHelper::~ShillClientHelper() {
  if (observer_list_.might_have_observers())
    NET_LOG(ERROR) << "ShillClientHelper destroyed with active observers";
}

void ShillClientHelper::SetReleasedCallback(ReleasedCallback callback) {
  CHECK(released_callback_.is_null());
  released_callback_ = callback;
}

void ShillClientHelper::AddPropertyChangedObserver(
    ShillPropertyChangedObserver* observer) {
  if (observer_list_.HasObserver(observer))
    return;
  AddRef();
  // Excecute all the pending MonitorPropertyChanged calls.
  for (size_t i = 0; i < interfaces_to_be_monitored_.size(); ++i) {
    MonitorPropertyChangedInternal(interfaces_to_be_monitored_[i]);
  }
  interfaces_to_be_monitored_.clear();

  observer_list_.AddObserver(observer);
}

void ShillClientHelper::RemovePropertyChangedObserver(
    ShillPropertyChangedObserver* observer) {
  if (!observer_list_.HasObserver(observer))
    return;
  observer_list_.RemoveObserver(observer);
  Release();
}

void ShillClientHelper::MonitorPropertyChanged(
    const std::string& interface_name) {
  if (observer_list_.might_have_observers()) {
    // Effectively monitor the PropertyChanged now.
    MonitorPropertyChangedInternal(interface_name);
  } else {
    // Delay the ConnectToSignal until an observer is added.
    interfaces_to_be_monitored_.push_back(interface_name);
  }
}

void ShillClientHelper::MonitorPropertyChangedInternal(
    const std::string& interface_name) {
  // We are not using dbus::PropertySet to monitor PropertyChanged signal
  // because the interface is not "org.freedesktop.DBus.Properties".
  proxy_->ConnectToSignal(interface_name, shill::kMonitorPropertyChanged,
                          base::Bind(&ShillClientHelper::OnPropertyChanged,
                                     weak_ptr_factory_.GetWeakPtr()),
                          base::BindOnce(&ShillClientHelper::OnSignalConnected,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void ShillClientHelper::CallVoidMethod(dbus::MethodCall* method_call,
                                       VoidDBusMethodCallback callback) {
  DCHECK(!callback.is_null());
  proxy_->CallMethod(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnVoidMethod,
                     base::Owned(new RefHolder(weak_ptr_factory_.GetWeakPtr())),
                     std::move(callback)));
}

void ShillClientHelper::CallObjectPathMethodWithErrorCallback(
    dbus::MethodCall* method_call,
    const ObjectPathCallback& callback,
    const ErrorCallback& error_callback) {
  DCHECK(!callback.is_null());
  DCHECK(!error_callback.is_null());
  proxy_->CallMethodWithErrorCallback(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnObjectPathMethodWithoutStatus,
                     base::Owned(new RefHolder(weak_ptr_factory_.GetWeakPtr())),
                     callback, error_callback),
      base::BindOnce(&OnError, error_callback));
}

void ShillClientHelper::CallDictionaryValueMethod(
    dbus::MethodCall* method_call,
    const DictionaryValueCallback& callback) {
  DCHECK(!callback.is_null());
  proxy_->CallMethod(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnDictionaryValueMethod,
                     base::Owned(new RefHolder(weak_ptr_factory_.GetWeakPtr())),
                     callback));
}

void ShillClientHelper::CallVoidMethodWithErrorCallback(
    dbus::MethodCall* method_call,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK(!callback.is_null());
  DCHECK(!error_callback.is_null());
  proxy_->CallMethodWithErrorCallback(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnVoidMethodWithErrorCallback,
                     base::Owned(new RefHolder(weak_ptr_factory_.GetWeakPtr())),
                     callback),
      base::BindOnce(&OnError, error_callback));
}

void ShillClientHelper::CallBooleanMethodWithErrorCallback(
    dbus::MethodCall* method_call,
    const BooleanCallback& callback,
    const ErrorCallback& error_callback) {
  DCHECK(!callback.is_null());
  DCHECK(!error_callback.is_null());
  proxy_->CallMethodWithErrorCallback(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnBooleanMethodWithErrorCallback,
                     base::Owned(new RefHolder(weak_ptr_factory_.GetWeakPtr())),
                     callback, error_callback),
      base::BindOnce(&OnError, error_callback));
}

void ShillClientHelper::CallStringMethodWithErrorCallback(
    dbus::MethodCall* method_call,
    const StringCallback& callback,
    const ErrorCallback& error_callback) {
  DCHECK(!callback.is_null());
  DCHECK(!error_callback.is_null());
  proxy_->CallMethodWithErrorCallback(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnStringMethodWithErrorCallback,
                     base::Owned(new RefHolder(weak_ptr_factory_.GetWeakPtr())),
                     callback, error_callback),
      base::BindOnce(&OnError, error_callback));
}

void ShillClientHelper::CallDictionaryValueMethodWithErrorCallback(
    dbus::MethodCall* method_call,
    const DictionaryValueCallbackWithoutStatus& callback,
    const ErrorCallback& error_callback) {
  DCHECK(!callback.is_null());
  DCHECK(!error_callback.is_null());
  proxy_->CallMethodWithErrorCallback(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnDictionaryValueMethodWithErrorCallback,
                     base::Owned(new RefHolder(weak_ptr_factory_.GetWeakPtr())),
                     callback, error_callback),
      base::BindOnce(&OnError, error_callback));
}

void ShillClientHelper::CallListValueMethodWithErrorCallback(
    dbus::MethodCall* method_call,
    const ListValueCallback& callback,
    const ErrorCallback& error_callback) {
  DCHECK(!callback.is_null());
  DCHECK(!error_callback.is_null());
  proxy_->CallMethodWithErrorCallback(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnListValueMethodWithErrorCallback,
                     base::Owned(new RefHolder(weak_ptr_factory_.GetWeakPtr())),
                     callback, error_callback),
      base::BindOnce(&OnError, error_callback));
}

namespace {

enum DictionaryType { DICTIONARY_TYPE_VARIANT, DICTIONARY_TYPE_STRING };

// Appends an a{ss} dictionary to |writer|. |dictionary| must only contain
// strings.
void AppendStringDictionary(const base::DictionaryValue& dictionary,
                            dbus::MessageWriter* writer) {
  dbus::MessageWriter variant_writer(NULL);
  writer->OpenVariant("a{ss}", &variant_writer);
  dbus::MessageWriter array_writer(NULL);
  variant_writer.OpenArray("{ss}", &array_writer);
  for (base::DictionaryValue::Iterator it(dictionary); !it.IsAtEnd();
       it.Advance()) {
    dbus::MessageWriter entry_writer(NULL);
    array_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(it.key());
    const base::Value& value = it.value();
    std::string value_string;
    if (!value.GetAsString(&value_string))
      NET_LOG(ERROR) << "Dictionary value not a string: " << it.key();
    entry_writer.AppendString(value_string);
    array_writer.CloseContainer(&entry_writer);
  }
  variant_writer.CloseContainer(&array_writer);
  writer->CloseContainer(&variant_writer);
}

// Implements AppendValueDataAsVariant. If |dictionary_type| is
// DICTIONARY_TYPE_VARIANT  and |value| is a Dictionary then it will be written
// as type 'a{ss}'. Otherwise dictionaries are written as type a{sv}. (This is
// to support Cellular.APN which expects a string -> string dictionary).
void AppendValueDataAsVariantInternal(dbus::MessageWriter* writer,
                                      const base::Value& value,
                                      DictionaryType dictionary_type) {
  // Support basic types and string-to-string dictionary.
  switch (value.type()) {
    case base::Value::Type::DICTIONARY: {
      const base::DictionaryValue* dictionary = NULL;
      value.GetAsDictionary(&dictionary);
      if (dictionary_type == DICTIONARY_TYPE_STRING) {
        AppendStringDictionary(*dictionary, writer);
      } else {
        dbus::MessageWriter variant_writer(NULL);
        writer->OpenVariant("a{sv}", &variant_writer);
        ShillClientHelper::AppendServicePropertiesDictionary(&variant_writer,
                                                             *dictionary);
        writer->CloseContainer(&variant_writer);
      }
      break;
    }
    case base::Value::Type::LIST: {
      const base::ListValue* list = NULL;
      value.GetAsList(&list);
      dbus::MessageWriter variant_writer(NULL);
      writer->OpenVariant("as", &variant_writer);
      dbus::MessageWriter array_writer(NULL);
      variant_writer.OpenArray("s", &array_writer);
      for (base::ListValue::const_iterator it = list->begin();
           it != list->end(); ++it) {
        const base::Value& value = *it;
        std::string value_string;
        if (!value.GetAsString(&value_string))
          NET_LOG(ERROR) << "List value not a string: " << value;
        array_writer.AppendString(value_string);
      }
      variant_writer.CloseContainer(&array_writer);
      writer->CloseContainer(&variant_writer);
      break;
    }
    case base::Value::Type::BOOLEAN:
    case base::Value::Type::INTEGER:
    case base::Value::Type::DOUBLE:
    case base::Value::Type::STRING:
      dbus::AppendBasicTypeValueDataAsVariant(writer, value);
      break;
    default:
      NET_LOG(ERROR) << "Unexpected value type: " << value.type();
  }
}

}  // namespace

// static
void ShillClientHelper::AppendValueDataAsVariant(dbus::MessageWriter* writer,
                                                 const base::Value& value) {
  AppendValueDataAsVariantInternal(writer, value, DICTIONARY_TYPE_VARIANT);
}

// static
void ShillClientHelper::AppendServicePropertiesDictionary(
    dbus::MessageWriter* writer,
    const base::DictionaryValue& dictionary) {
  dbus::MessageWriter array_writer(NULL);
  writer->OpenArray("{sv}", &array_writer);
  for (base::DictionaryValue::Iterator it(dictionary); !it.IsAtEnd();
       it.Advance()) {
    dbus::MessageWriter entry_writer(NULL);
    array_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(it.key());
    // Shill expects Cellular.APN to be a string dictionary, a{ss}. All other
    // properties use a varient dictionary, a{sv}. TODO(stevenjb): Remove this
    // hack if/when we change Shill to accept a{sv} for Cellular.APN.
    DictionaryType dictionary_type = (it.key() == shill::kCellularApnProperty)
                                         ? DICTIONARY_TYPE_STRING
                                         : DICTIONARY_TYPE_VARIANT;
    AppendValueDataAsVariantInternal(&entry_writer, it.value(),
                                     dictionary_type);
    array_writer.CloseContainer(&entry_writer);
  }
  writer->CloseContainer(&array_writer);
}

void ShillClientHelper::AddRef() {
  ++active_refs_;
}

void ShillClientHelper::Release() {
  --active_refs_;
  if (active_refs_ == 0 && !released_callback_.is_null())
    std::move(released_callback_).Run(this);  // May delete this
}

void ShillClientHelper::OnSignalConnected(const std::string& interface,
                                          const std::string& signal,
                                          bool success) {
  if (!success)
    NET_LOG(ERROR) << "Connect to " << interface << " " << signal << " failed.";
}

void ShillClientHelper::OnPropertyChanged(dbus::Signal* signal) {
  if (!observer_list_.might_have_observers())
    return;

  dbus::MessageReader reader(signal);
  std::string name;
  if (!reader.PopString(&name))
    return;
  std::unique_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  if (!value.get())
    return;

  for (auto& observer : observer_list_)
    observer.OnPropertyChanged(name, *value);
}

}  // namespace chromeos
