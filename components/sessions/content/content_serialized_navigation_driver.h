// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CONTENT_CONTENT_SERIALIZED_NAVIGATION_DRIVER_H_
#define COMPONENTS_SESSIONS_CONTENT_CONTENT_SERIALIZED_NAVIGATION_DRIVER_H_

#include <map>
#include <memory>
#include <string>

#include "components/sessions/content/extended_info_handler.h"
#include "components/sessions/core/serialized_navigation_driver.h"
#include "components/sessions/core/sessions_export.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

namespace sessions {

// Provides an implementation of SerializedNavigationDriver that is backed by
// content classes.
class SESSIONS_EXPORT ContentSerializedNavigationDriver
    : public SerializedNavigationDriver {
 public:
  ContentSerializedNavigationDriver(const ContentSerializedNavigationDriver&) =
      delete;
  ContentSerializedNavigationDriver& operator=(
      const ContentSerializedNavigationDriver&) = delete;

  ~ContentSerializedNavigationDriver() override;

  // Returns the singleton ContentSerializedNavigationDriver.  Almost all
  // callers should use SerializedNavigationDriver::Get() instead.
  static ContentSerializedNavigationDriver* GetInstance();

  // Allows an embedder to override the instance returned by GetInstance().
  static void SetInstance(ContentSerializedNavigationDriver* instance);

  // SerializedNavigationDriver implementation.
  int GetDefaultReferrerPolicy() const override;
  std::string GetSanitizedPageStateForPickle(
      const SerializedNavigationEntry* navigation) const override;
  void Sanitize(SerializedNavigationEntry* navigation) const override;
  std::string StripReferrerFromPageState(
      const std::string& page_state) const override;

  // Registers a handler that is used to read and write the extended
  // info stored in SerializedNavigationEntry. As part of serialization |key|
  // is written to disk, as such once a handler is registered it should always
  // be registered to the same key.
  void RegisterExtendedInfoHandler(
      const std::string& key,
      std::unique_ptr<ExtendedInfoHandler> handler);

  using ExtendedInfoHandlerMap =
      std::map<std::string, std::unique_ptr<ExtendedInfoHandler>>;

  // Returns all the registered handlers to deal with the extended info.
  const ExtendedInfoHandlerMap& GetAllExtendedInfoHandlers() const;

 protected:
  ContentSerializedNavigationDriver();

 private:
  friend struct base::DefaultSingletonTraits<ContentSerializedNavigationDriver>;
  friend class ContentSerializedNavigationBuilderTest;

  ExtendedInfoHandlerMap extended_info_handler_map_;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CONTENT_CONTENT_SERIALIZED_NAVIGATION_DRIVER_H_
