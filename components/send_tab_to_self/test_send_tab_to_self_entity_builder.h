// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_TEST_SEND_TAB_TO_SELF_ENTITY_BUILDER_H_
#define COMPONENTS_SEND_TAB_TO_SELF_TEST_SEND_TAB_TO_SELF_ENTITY_BUILDER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/time/time.h"
#include "url/gurl.h"

namespace syncer {
class LoopbackServerEntity;
}  // namespace syncer

namespace fake_server {
class FakeServer;
}  // namespace fake_server

namespace send_tab_to_self {

// Builder for constructing Send Tab to Self entities for `FakeServer` in tests.
// Encapsulates GUID generation, page context packaging, and Nigori encryption
// of `PageContext` fields.
class TestSendTabToSelfEntityBuilder {
 public:
  static constexpr std::string_view kDefaultTitle = "Default Title";
  static constexpr std::string_view kDefaultDeviceName = "target_device";

  // Constructs a builder with `url` and optional `title`.
  explicit TestSendTabToSelfEntityBuilder(
      GURL url,
      std::string title = std::string(kDefaultTitle));
  ~TestSendTabToSelfEntityBuilder();

  TestSendTabToSelfEntityBuilder(const TestSendTabToSelfEntityBuilder&) =
      delete;
  TestSendTabToSelfEntityBuilder& operator=(
      const TestSendTabToSelfEntityBuilder&) = delete;

  TestSendTabToSelfEntityBuilder(TestSendTabToSelfEntityBuilder&&);
  TestSendTabToSelfEntityBuilder& operator=(TestSendTabToSelfEntityBuilder&&);

  // Sets the title of the entity.
  TestSendTabToSelfEntityBuilder& SetTitle(std::string title);

  // Sets the unique GUID of the entity. If omitted, a random UUID is generated.
  TestSendTabToSelfEntityBuilder& SetGuid(std::string guid);

  // Sets the sender device name. Defaults to `kDefaultDeviceName`.
  TestSendTabToSelfEntityBuilder& SetDeviceName(std::string device_name);

  // Sets the target device's sync cache GUID.
  TestSendTabToSelfEntityBuilder& SetTargetDeviceCacheGuid(
      std::string target_guid);

  // Sets the shared timestamp. If omitted, defaults to `base::Time::Now()`.
  TestSendTabToSelfEntityBuilder& SetSharedTime(base::Time time);

  // Sets whether the entry has been opened on the receiving side.
  TestSendTabToSelfEntityBuilder& SetOpened(bool opened);

  // Sets whether the notification for this entry was dismissed.
  TestSendTabToSelfEntityBuilder& SetNotificationDismissed(bool dismissed);

  struct FormField {
    std::string id_attribute;
    std::string name_attribute;
    std::string value;
  };

  // Adds a form field to the entry's `PageContext` with `id_attribute` and
  // optional `name_attribute` (defaults to empty string).
  TestSendTabToSelfEntityBuilder& AddFormField(
      std::string_view id_attribute,
      std::string_view value,
      std::string_view name_attribute = "");

  // Sets the text fragment selector string for scroll position restoration.
  TestSendTabToSelfEntityBuilder& SetTextFragment(
      std::string_view text_fragment);

  // Builds the `syncer::LoopbackServerEntity` representing this Send Tab to
  // Self entry. If `PageContext` fields (`AddFormField()` or
  // `SetTextFragment()`) are populated, `fake_server` must have Nigori
  // keystore keys so that `PageContext` is encrypted into
  // `encrypted_page_context` (since `DataTypeWorker` strips unencrypted
  // `page_context` fields from incoming remote entities on the wire).
  std::unique_ptr<syncer::LoopbackServerEntity> Build(
      const fake_server::FakeServer& fake_server) const;

  const GURL& url() const { return url_; }
  const std::string& title() const { return title_; }
  const std::string& guid() const { return guid_; }
  const std::string& device_name() const { return device_name_; }
  const std::string& target_device_cache_guid() const {
    return target_device_cache_guid_;
  }

 private:
  GURL url_;
  std::string title_;
  std::string guid_;
  std::string device_name_ = std::string(kDefaultDeviceName);
  std::string target_device_cache_guid_;
  base::Time shared_time_;
  bool opened_ = false;
  bool notification_dismissed_ = false;
  std::vector<FormField> form_fields_;
  std::string text_fragment_;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_TEST_SEND_TAB_TO_SELF_ENTITY_BUILDER_H_
