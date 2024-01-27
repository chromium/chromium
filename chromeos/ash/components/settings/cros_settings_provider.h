// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SETTINGS_CROS_SETTINGS_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_SETTINGS_CROS_SETTINGS_PROVIDER_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/functional/callback.h"

namespace base {
class Value;
}  // namespace base

namespace ash {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SETTINGS) CrosSettingsProvider {
 public:
  // The callback type that is called to notify the CrosSettings observers
  // about a setting change.
  using NotifyObserversCallback =
      base::RepeatingCallback<void(const std::string&)>;

  // Possible results of a trusted check.
  enum TrustedStatus {
    // The trusted values were populated in the cache and can be accessed
    // until the next iteration of the message loop.
    TRUSTED,
    // Either a store or a load operation is in progress. The provided
    // callback will be invoked once the verification has finished.
    TEMPORARILY_UNTRUSTED,
    // The verification of the trusted store has failed permanently. The
    // client should assume this state final and further checks for
    // trustedness will fail at least until the browser restarts.
    PERMANENTLY_UNTRUSTED,
  };

  // Creates a new provider instance. |notify_cb| will be used to notify
  // about setting changes.
  explicit CrosSettingsProvider(const NotifyObserversCallback& notify_cb);
  virtual ~CrosSettingsProvider();

  // Gets settings value of given |path| to |out_value|.
  virtual const base::Value* Get(std::string_view path) const = 0;

  // Requests the provider to fetch its values from a trusted store, if it
  // hasn't done so yet. Returns TRUSTED if the values returned by this provider
  // are trusted during the current loop cycle. Otherwise returns
  // TEMPORARILY_UNTRUSTED, takes ownership of |callback|, and will invoke it
  // later when trusted values become available. PrepareTrustedValues() should
  // be tried again in that case. Returns PERMANENTLY_UNTRUSTED if a permanent
  // error has occurred.
  virtual TrustedStatus PrepareTrustedValues(base::OnceClosure* callback) = 0;

  // Gets the namespace prefix provided by this provider.
  virtual bool HandlesSetting(std::string_view path) const = 0;

  void SetNotifyObserversCallback(const NotifyObserversCallback& notify_cb);

 protected:
  // Notifies the observers about a setting change.
  void NotifyObservers(const std::string& path);

 private:
  // Callback used to notify about setting changes.
  NotifyObserversCallback notify_cb_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SETTINGS_CROS_SETTINGS_PROVIDER_H_
