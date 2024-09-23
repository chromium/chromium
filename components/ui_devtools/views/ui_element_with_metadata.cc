// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/ui_element_with_metadata.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/ui_devtools/views/element_utility.h"
#include "ui/base/metadata/metadata_types.h"

namespace ui_devtools {

namespace {

// Remove any custom editor "prefixes" from the property name. The prefixes must
// not be valid identifier characters.
void StripPrefix(std::string& property_name) {
  auto cur = property_name.cbegin();
  for (; cur < property_name.cend(); ++cur) {
    if ((*cur >= 'A' && *cur <= 'Z') || (*cur >= 'a' && *cur <= 'z') ||
        *cur == '_') {
      break;
    }
  }
  property_name.erase(property_name.cbegin(), cur);
}

}  // namespace

UIElementWithMetaData::UIElementWithMetaData(const UIElementType type,
                                             UIElementDelegate* delegate,
                                             UIElement* parent)
    : UIElement(type, delegate, parent) {}

UIElementWithMetaData::~UIElementWithMetaData() = default;

std::vector<UIElement::ClassProperties>
UIElementWithMetaData::GetCustomPropertiesForMatchedStyle() const {
  std::vector<UIElement::ClassProperties> ret;
  std::vector<UIElement::UIProperty> class_properties;

  ui::Layer* layer = GetLayer();
  if (layer) {
    AppendLayerPropertiesMatchedStyle(layer, &class_properties);
    ret.emplace_back("Layer", class_properties);
    class_properties.clear();
  }

  ui::metadata::ClassMetaData* metadata = GetClassMetaData();
  void* instance = GetClassInstance();
  for (auto member = metadata->begin(); member != metadata->end(); member++) {
    auto flags = (*member)->GetPropertyFlags();
    if (!!(flags & ui::metadata::PropertyFlags::kSerializable) ||
        !!(flags & ui::metadata::PropertyFlags::kReadOnly)) {
      class_properties.emplace_back(
          (*member)->GetMemberNamePrefix() + (*member)->member_name(),
          base::UTF16ToUTF8((*member)->GetValueAsString(instance)));
    }

    if (member.IsLastMember()) {
      ret.emplace_back(member.GetCurrentCollectionName(), class_properties);
      class_properties.clear();
    }
  }
  return ret;
}

void UIElementWithMetaData::GetVisible(bool* visible) const {
  // Visibility information should be directly retrieved from element's
  // metadata, no need for this function any more.
  NOTREACHED_IN_MIGRATION();
}

void UIElementWithMetaData::SetVisible(bool visible) {
  // Intentional No-op.
}

bool UIElementWithMetaData::SetPropertiesFromString(const std::string& text) {
  bool property_set = false;
  std::vector<std::string> tokens = base::SplitString(
      text, ":;", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (tokens.size() == 0UL)
    return false;

  ui::metadata::ClassMetaData* metadata = GetClassMetaData();
  void* instance = GetClassInstance();

  for (size_t i = 0; i < tokens.size() - 1; i += 2) {
    std::string property_name = tokens.at(i);
    std::string property_value = base::ToLowerASCII(tokens.at(i + 1));

    // Remove any type editor "prefixes" from the property name.
    StripPrefix(property_name);

    ui::metadata::MemberMetaDataBase* member =
        metadata->FindMemberData(property_name);
    if (!member) {
      DLOG(ERROR) << "UI DevTools: Can not find property " << property_name
                  << " in MetaData.";
      continue;
    }

    // Since DevTools frontend doesn't check the value, we do a sanity check
    // based on the allowed values specified in the metadata.
    auto valid_values = member->GetValidValues();
    if (!valid_values.empty() &&
        !base::Contains(valid_values, base::UTF8ToUTF16(property_value))) {
      // Ignore the value.
      continue;
    }

    auto property_flags = member->GetPropertyFlags();
    if (!!(property_flags & ui::metadata::PropertyFlags::kReadOnly))
      continue;
    DCHECK(!!(property_flags & ui::metadata::PropertyFlags::kSerializable));
    member->SetValueAsString(instance, base::UTF8ToUTF16(property_value));
    property_set = true;
  }

  return property_set;
}

void UIElementWithMetaData::InitSources() {
  if (GetLayer())
    AddSource("ui/compositor/layer.h", 0);

  for (ui::metadata::ClassMetaData* metadata = GetClassMetaData();
       metadata != nullptr; metadata = metadata->parent_class_meta_data()) {
    // If class has Metadata properties, add their sources.
    if (!metadata->members().empty()) {
      AddSource(metadata->file(), metadata->line());
    }
  }
}

ui::Layer* UIElementWithMetaData::GetLayer() const {
  return nullptr;
}

}  // namespace ui_devtools
