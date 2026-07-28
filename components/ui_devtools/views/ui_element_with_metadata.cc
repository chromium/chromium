// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/ui_element_with_metadata.h"

#include <algorithm>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
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

std::vector<UIElement::PropertyGroup> UIElementWithMetaData::GetPropertyGroups()
    const {
  std::vector<UIElement::PropertyGroup> groups;
  std::vector<UIElement::UIProperty> class_properties;

  ui::Layer* layer = GetLayer();
  if (layer) {
    std::vector<UIElement::UIProperty> layer_props;
    auto instance_getter = base::BindRepeating(
        [](const UIElementWithMetaData* el) -> void* {
          return el ? el->GetLayer() : nullptr;
        },
        base::Unretained(this));

    AppendLayerPropertiesMatchedStyle(layer, &layer_props);

    auto custom_setter = base::BindRepeating(
        [](const UIElementWithMetaData* el, const std::string& name,
           const std::string& value) -> bool {
          if (!el || !el->GetLayer()) {
            return false;
          }
          return SetLayerPropertyFromString(el->GetLayer(), name, value);
        },
        base::Unretained(this));

    groups.emplace_back("Layer", instance_getter, layer_props, custom_setter,
                        base::RepeatingClosure());
  }

  ui::metadata::ClassMetaData* metadata = GetClassMetaData();
  void* instance = GetClassInstance();
  if (metadata && instance) {
    for (auto member = metadata->begin(); member != metadata->end(); member++) {
      auto flags = (*member)->GetPropertyFlags();
      if (!!(flags & ui::metadata::PropertyFlags::kSerializable) ||
          !!(flags & ui::metadata::PropertyFlags::kReadOnly)) {
        class_properties.emplace_back(
            base::StrCat(
                {(*member)->GetMemberNamePrefix(), (*member)->member_name()}),
            base::UTF16ToUTF8((*member)->GetValueAsString(instance)), *member,
            base::BindRepeating([](void* inst) -> void* { return inst; },
                                instance));
      }

      if (member.IsLastMember()) {
        groups.emplace_back(
            std::string(member.GetCurrentCollectionName()),
            base::BindRepeating([](void* inst) -> void* { return inst; },
                                instance),
            metadata, class_properties);
        class_properties.clear();
      }
    }
  }
  return groups;
}

std::vector<UIElement::ClassProperties>
UIElementWithMetaData::GetCustomPropertiesForMatchedStyle() const {
  std::vector<UIElement::ClassProperties> ret;
  std::vector<PropertyGroup> groups = GetPropertyGroups();
  for (const auto& group : groups) {
    ret.emplace_back(group.group_name_, group.properties_);
  }
  return ret;
}

void UIElementWithMetaData::GetVisible(bool* visible) const {
  // Visibility information should be directly retrieved from element's
  // metadata, no need for this function any more.
  NOTREACHED();
}

void UIElementWithMetaData::SetVisible(bool visible) {
  // Intentional No-op.
}

bool UIElementWithMetaData::SetPropertiesFromString(size_t group_index,
                                                    const std::string& text) {
  std::vector<PropertyGroup> groups = GetPropertyGroups();
  if (group_index >= groups.size()) {
    return false;
  }

  std::vector<std::string> tokens = base::SplitString(
      text, ":;", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (tokens.empty()) {
    return false;
  }

  auto apply_edits_to_group = [&](const PropertyGroup& group) -> bool {
    void* instance = group.GetInstance();
    if (!instance && group.class_metadata_) {
      DLOG(WARNING) << "UI DevTools: Target object instance for group "
                    << group.group_name_ << " no longer exists.";
      return false;
    }

    bool group_set = false;
    for (size_t i = 0; i < tokens.size() - 1; i += 2) {
      std::string property_name = tokens.at(i);
      std::string property_value = base::ToLowerASCII(tokens.at(i + 1));

      // Remove any type editor "prefixes" from the property name.
      StripPrefix(property_name);

      if (group.class_metadata_) {
        ui::metadata::MemberMetaDataBase* member =
            group.class_metadata_->FindMemberData(property_name);
        if (!member) {
          for (auto m = group.class_metadata_->begin();
               m != group.class_metadata_->end(); ++m) {
            if (base::EqualsCaseInsensitiveASCII((*m)->member_name(),
                                                 property_name)) {
              member = *m;
              break;
            }
          }
        }
        if (!member) {
          DLOG(ERROR) << "UI DevTools: Can not find property " << property_name
                      << " in MetaData for " << group.group_name_;
          continue;
        }

        auto valid_values = member->GetValidValues();
        std::u16string target_value = base::UTF8ToUTF16(property_value);
        if (!valid_values.empty()) {
          auto it =
              std::ranges::find_if(valid_values, [&](const std::u16string& v) {
                return base::EqualsCaseInsensitiveASCII(v, target_value);
              });
          if (it == valid_values.end()) {
            continue;
          }
          target_value = *it;
        }

        auto property_flags = member->GetPropertyFlags();
        if (!!(property_flags & ui::metadata::PropertyFlags::kReadOnly)) {
          continue;
        }
        DCHECK(!!(property_flags & ui::metadata::PropertyFlags::kSerializable));
        member->SetValueAsString(instance, target_value);
        group_set = true;
      } else if (group.custom_setter_) {
        group_set |= group.custom_setter_.Run(property_name, property_value);
      }
    }

    if (group_set && group.on_changed_callback_) {
      group.on_changed_callback_.Run();
    }
    return group_set;
  };

  return apply_edits_to_group(groups[group_index]);
}

void UIElementWithMetaData::InitSources() {
  if (GetLayer())
    AddSource("ui/compositor/layer.h", 0);

  for (ui::metadata::ClassMetaData* metadata = GetClassMetaData();
       metadata != nullptr; metadata = metadata->parent_class_meta_data()) {
    // If class has Metadata properties, add their sources.
    if (!metadata->members().empty()) {
      AddSource(std::string(metadata->file()), metadata->line());
    }
  }
}

ui::Layer* UIElementWithMetaData::GetLayer() const {
  return nullptr;
}

}  // namespace ui_devtools
