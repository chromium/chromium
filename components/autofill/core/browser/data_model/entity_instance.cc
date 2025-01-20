// Copyright 2025 The Chromium Authors
// // Use of this source code is governed by a BSD-style license that can be
// // found in the LICENSE file.
//
#include "components/autofill/core/browser/data_model/entity_instance.h"

namespace autofill {

AttributeInstance::Context::Context() = default;
AttributeInstance::Context::Context(const AttributeInstance::Context&) =
    default;
AttributeInstance::Context& AttributeInstance::Context::operator=(
    const AttributeInstance::Context&) = default;
AttributeInstance::Context::Context(AttributeInstance::Context&&) = default;
AttributeInstance::Context& AttributeInstance::Context::operator=(
    AttributeInstance::Context&&) = default;
AttributeInstance::Context::~Context() = default;

AttributeInstance::AttributeInstance(AttributeType type,
                                     std::string value,
                                     Context context)
    : type_(type), value_(std::move(value)), context_(std::move(context)) {}

AttributeInstance::AttributeInstance(const AttributeInstance&) = default;
AttributeInstance& AttributeInstance::operator=(const AttributeInstance&) =
    default;
AttributeInstance::AttributeInstance(AttributeInstance&&) = default;
AttributeInstance& AttributeInstance::operator=(AttributeInstance&&) = default;
AttributeInstance::~AttributeInstance() = default;

EntityInstance::EntityInstance(
    EntityType type,
    base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
        attributes,
    base::Uuid guid,
    std::string nickname,
    base::Time date_modified)
    : type_(type),
      attributes_(std::move(attributes)),
      guid_(std::move(guid)),
      nickname_(std::move(nickname)),
      date_modified_(date_modified) {}

EntityInstance::EntityInstance(const EntityInstance&) = default;
EntityInstance& EntityInstance::operator=(const EntityInstance&) = default;
EntityInstance::EntityInstance(EntityInstance&&) = default;
EntityInstance& EntityInstance::operator=(EntityInstance&&) = default;
EntityInstance::~EntityInstance() = default;

std::ostream& operator<<(std::ostream& os, const AttributeInstance& a) {
  os << a.type() << ": " << '"' << a.value() << '"';
  return os;
}

std::ostream& operator<<(std::ostream& os, const EntityInstance& e) {
  os << "- name: " << '"' << e.type() << '"' << std::endl;
  os << "- nickname: " << '"' << e.nickname() << '"' << std::endl;
  os << "- guid: " << '"' << e.guid().AsLowercaseString() << '"' << std::endl;
  os << "- date modified: " << '"' << e.date_modified() << '"' << std::endl;
  for (const AttributeInstance& a : e.attributes()) {
    os << "- attribute " << a << std::endl;
  }
  return os;
}

}  // namespace autofill
