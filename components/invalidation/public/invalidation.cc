// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/invalidation.h"

#include <cstddef>

#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/invalidation/public/ack_handler.h"
#include "components/invalidation/public/invalidation_util.h"

namespace invalidation {

namespace {

const char kObjectIdKey[] = "objectId";
const char kIsUnknownVersionKey[] = "isUnknownVersion";
const char kVersionKey[] = "version";
const char kPayloadKey[] = "payload";
const int64_t kInvalidVersion = -1;

// Fills base::Value::Dict as if legacy ObjectID still would be in use.
// Used to provide values for chrome://invalidations page.
base::Value::Dict TopicToObjectIDValue(const Topic& topic) {
  base::Value::Dict value;
  // Source has been deprecated, pass 0 instead.
  value.Set("source", 0);
  value.Set("name", topic);
  return value;
}

}  // namespace

// static
Invalidation Invalidation::Init(const Topic& topic,
                                int64_t version,
                                const std::string& payload) {
  return Invalidation(topic, /*is_unknown_version=*/false, version, payload,
                      AckHandle::CreateUnique());
}

// static
Invalidation Invalidation::InitUnknownVersion(const Topic& topic) {
  return Invalidation(topic, /*is_unknown_version=*/true, kInvalidVersion,
                      std::string(), AckHandle::CreateUnique());
}

// static
Invalidation Invalidation::InitFromDroppedInvalidation(
    const Invalidation& dropped) {
  return Invalidation(dropped.topic(), /*is_unknown_version=*/true,
                      kInvalidVersion, std::string(), dropped.ack_handle_);
}

Invalidation::Invalidation(const Invalidation& other) = default;

Invalidation& Invalidation::operator=(const Invalidation& other) = default;

Invalidation::~Invalidation() = default;

Topic Invalidation::topic() const {
  return topic_;
}

bool Invalidation::is_unknown_version() const {
  return is_unknown_version_;
}

int64_t Invalidation::version() const {
  DCHECK(!is_unknown_version_);
  return version_;
}

const std::string& Invalidation::payload() const {
  DCHECK(!is_unknown_version_);
  return payload_;
}

const AckHandle& Invalidation::ack_handle() const {
  return ack_handle_;
}

void Invalidation::SetAckHandler(
    base::WeakPtr<AckHandler> handler,
    scoped_refptr<base::SequencedTaskRunner> handler_task_runner) {
  ack_handler_ = handler;
  ack_handler_task_runner_ = handler_task_runner;
}

bool Invalidation::SupportsAcknowledgement() const {
  return !!ack_handler_task_runner_;
}

void Invalidation::Acknowledge() const {
  if (SupportsAcknowledgement()) {
    ack_handler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AckHandler::Acknowledge, ack_handler_,
                                  topic(), ack_handle_));
  }
}

void Invalidation::Drop() {
  if (SupportsAcknowledgement()) {
    ack_handler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AckHandler::Drop, ack_handler_, topic(), ack_handle_));
  }
}

bool Invalidation::operator==(const Invalidation& other) const {
  return topic_ == other.topic_ &&
         is_unknown_version_ == other.is_unknown_version_ &&
         version_ == other.version_ && payload_ == other.payload_;
}

base::Value::Dict Invalidation::ToValue() const {
  base::Value::Dict value;
  // TODO(crbug.com/1056181): ObjectID has been deprecated, but the value here
  // used in the js counterpart (chrome://invalidations). Replace ObjectID with
  // Topic here together with js counterpart update.
  value.Set(kObjectIdKey, TopicToObjectIDValue(topic_));
  if (is_unknown_version_) {
    value.Set(kIsUnknownVersionKey, true);
  } else {
    value.Set(kIsUnknownVersionKey, false);
    value.Set(kVersionKey, base::NumberToString(version_));
    value.Set(kPayloadKey, payload_);
  }
  return value;
}

std::string Invalidation::ToString() const {
  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(true);
  serializer.Serialize(ToValue());
  return output;
}

Invalidation::Invalidation(const Topic& topic,
                           bool is_unknown_version,
                           int64_t version,
                           const std::string& payload,
                           AckHandle ack_handle)
    : topic_(topic),
      is_unknown_version_(is_unknown_version),
      version_(version),
      payload_(payload),
      ack_handle_(ack_handle) {}

}  // namespace invalidation
