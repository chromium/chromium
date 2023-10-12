// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/invalidation.h"

#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/invalidation/public/ack_handler.h"
#include "components/invalidation/public/invalidation_util.h"

namespace invalidation {

namespace {

const char kTopic[] = "topic";
const char kVersionKey[] = "version";
const char kPayloadKey[] = "payload";

}  // namespace

// static
Invalidation Invalidation::Init(const Topic& topic,
                                int64_t version,
                                const std::string& payload) {
  return Invalidation(topic, version, payload, AckHandle::CreateUnique());
}

Invalidation::Invalidation(const Invalidation& other) = default;

Invalidation& Invalidation::operator=(const Invalidation& other) = default;

Invalidation::~Invalidation() = default;

Topic Invalidation::topic() const {
  return topic_;
}

int64_t Invalidation::version() const {
  return version_;
}

const std::string& Invalidation::payload() const {
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

bool Invalidation::operator==(const Invalidation& other) const {
  return topic_ == other.topic_ &&
         version_ == other.version_ && payload_ == other.payload_;
}

base::Value::Dict Invalidation::ToValue() const {
  base::Value::Dict value;
  value.Set(kTopic, topic_);
  value.Set(kVersionKey, base::NumberToString(version_));
  value.Set(kPayloadKey, payload_);
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
                           int64_t version,
                           const std::string& payload,
                           AckHandle ack_handle)
    : topic_(topic),
      version_(version),
      payload_(payload),
      ack_handle_(ack_handle) {}

}  // namespace invalidation
