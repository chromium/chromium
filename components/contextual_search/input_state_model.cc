// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/input_state_model.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/pref_names.h"
#include "components/lens/contextual_input.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "net/base/url_util.h"
#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/rule_set.pb.h"
#include "url/gurl.h"

namespace contextual_search {

using omnibox::SearchboxConfig;

namespace {

// Populates `InputTypeRule` for `omnibox::INPUT_TYPE_BROWSER_TAB` if it does
// not exist.
void MaybePopulateBrowserTabInputTypeRule(omnibox::SearchboxConfig* config) {
  if (!config) {
    return;
  }
  omnibox::RuleSet* rule_set = config->mutable_rule_set();

  bool browser_tab_rule_exists =
      std::ranges::any_of(rule_set->input_type_rules(), [](const auto& rule) {
        return rule.input_type() == omnibox::INPUT_TYPE_BROWSER_TAB;
      });

  if (browser_tab_rule_exists) {
    return;
  }

  // Populate `InputTypeRule` for `omnibox::INPUT_TYPE_BROWSER_TAB`.
  omnibox::InputTypeRule* new_rule = rule_set->add_input_type_rules();
  new_rule->set_input_type(omnibox::INPUT_TYPE_BROWSER_TAB);
  new_rule->add_allowed_input_types(omnibox::INPUT_TYPE_LENS_IMAGE);
  new_rule->add_allowed_input_types(omnibox::INPUT_TYPE_LENS_FILE);
  new_rule->add_allowed_input_types(omnibox::INPUT_TYPE_BROWSER_TAB);

  // Add `omnibox::INPUT_TYPE_BROWSER_TAB` to the `allowed_input_types` in
  // `ToolRule` for all tools if the tool allows both images and files.
  for (auto& tool_rule : *rule_set->mutable_tool_rules()) {
    bool has_image = false;
    bool has_file = false;
    for (const auto& input_type : tool_rule.allowed_input_types()) {
      if (input_type == omnibox::INPUT_TYPE_LENS_IMAGE) {
        has_image = true;
      } else if (input_type == omnibox::INPUT_TYPE_LENS_FILE) {
        has_file = true;
      }
    }
    if (has_image && has_file) {
      tool_rule.add_allowed_input_types(omnibox::INPUT_TYPE_BROWSER_TAB);
    }
  }

  // Add `omnibox::INPUT_TYPE_BROWSER_TAB` to the `allowed_input_types` in
  // `ModelRule` for all models if the model allows both images and files.
  for (auto& model_rule : *rule_set->mutable_model_rules()) {
    bool has_image = false;
    bool has_file = false;
    for (const auto& input_type : model_rule.allowed_input_types()) {
      if (input_type == omnibox::INPUT_TYPE_LENS_IMAGE) {
        has_image = true;
      } else if (input_type == omnibox::INPUT_TYPE_LENS_FILE) {
        has_file = true;
      }
    }
    if (has_image && has_file) {
      model_rule.add_allowed_input_types(omnibox::INPUT_TYPE_BROWSER_TAB);
    }
  }
}

// Populates `InputTypeRule` for `omnibox::INPUT_TYPE_DRIVE` if it does
// not exist and the signin promo feature is enabled on DICE platforms.
// This option is available even on signout, which will prompt the signin promo
// when clicked.
void MaybePopulateDriveInputTypeRule(omnibox::SearchboxConfig* config) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (!config || !base::FeatureList::IsEnabled(
                     omnibox::kComposeboxDriveContextMenuOptionSigninPromo)) {
    return;
  }
  omnibox::RuleSet* rule_set = config->mutable_rule_set();

  bool drive_rule_exists =
      std::ranges::any_of(rule_set->input_type_rules(), [](const auto& rule) {
        return rule.input_type() == omnibox::INPUT_TYPE_DRIVE;
      });

  if (drive_rule_exists) {
    return;
  }

  // Populate `InputTypeRule` for `omnibox::INPUT_TYPE_DRIVE`.
  omnibox::InputTypeRule* new_rule = rule_set->add_input_type_rules();
  new_rule->set_input_type(omnibox::INPUT_TYPE_DRIVE);
  new_rule->add_allowed_input_types(omnibox::INPUT_TYPE_DRIVE);

  for (auto& tool_rule : *rule_set->mutable_tool_rules()) {
    if (!std::ranges::contains(tool_rule.allowed_input_types(),
                               omnibox::INPUT_TYPE_DRIVE)) {
      tool_rule.add_allowed_input_types(omnibox::INPUT_TYPE_DRIVE);
    }
  }
  for (auto& model_rule : *rule_set->mutable_model_rules()) {
    if (!std::ranges::contains(model_rule.allowed_input_types(),
                               omnibox::INPUT_TYPE_DRIVE)) {
      model_rule.add_allowed_input_types(omnibox::INPUT_TYPE_DRIVE);
    }
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

std::optional<omnibox::ModelMode> GetActiveModelFromUrl(
    const GURL& active_url,
    const std::vector<omnibox::ModelConfig>& model_configs,
    const std::vector<omnibox::ModelMode>& allowed_models) {
  if (!active_url.is_valid() || !active_url.has_query()) {
    return std::nullopt;
  }

  std::optional<omnibox::ModelMode> best_model = std::nullopt;
  size_t max_matched_params = 0;

  for (const auto& model_config : model_configs) {
    if (!std::ranges::contains(allowed_models, model_config.model())) {
      continue;
    }
    if (model_config.aim_url_params().empty()) {
      continue;
    }
    bool all_params_match = true;
    size_t matched_count = model_config.aim_url_params().size();
    for (const auto& url_param : model_config.aim_url_params()) {
      std::string value;
      bool found =
          net::GetValueForKeyInQuery(active_url, url_param.param_key(), &value);
      if (!found || value != url_param.param_value()) {
        all_params_match = false;
        break;
      }
    }
    if (all_params_match) {
      if (!best_model.has_value() || matched_count > max_matched_params) {
        best_model = model_config.model();
        max_matched_params = matched_count;
      } else if (matched_count == max_matched_params) {
        DLOG(WARNING) << "Ambiguous model match tie!";
      }
    }
  }

  return best_model;
}

std::optional<omnibox::ToolMode> GetActiveToolFromUrl(
    const GURL& active_url,
    const std::vector<omnibox::ToolConfig>& tool_configs,
    const std::vector<omnibox::ToolMode>& allowed_tools) {
  if (!active_url.is_valid() || !active_url.has_query()) {
    return std::nullopt;
  }

  std::optional<omnibox::ToolMode> best_tool = std::nullopt;
  size_t max_matched_params = 0;

  for (const auto& tool_config : tool_configs) {
    if (!std::ranges::contains(allowed_tools, tool_config.tool())) {
      continue;
    }
    if (tool_config.aim_url_params().empty()) {
      continue;
    }
    bool all_params_match = true;
    size_t matched_count = tool_config.aim_url_params().size();
    for (const auto& url_param : tool_config.aim_url_params()) {
      std::string value;
      bool found =
          net::GetValueForKeyInQuery(active_url, url_param.param_key(), &value);
      if (!found || value != url_param.param_value()) {
        all_params_match = false;
        break;
      }
    }
    if (all_params_match) {
      if (!best_tool.has_value() || matched_count > max_matched_params) {
        best_tool = tool_config.tool();
        max_matched_params = matched_count;
      } else if (matched_count == max_matched_params) {
        DLOG(WARNING) << "Ambiguous tool match tie!";
      }
    }
  }

  return best_tool;
}

// Checks if a set of items are all present in an allowed list.
template <typename T, typename U>
bool AreItemsAllowed(const T& items, const U& allowed_items) {
  return std::all_of(items.begin(), items.end(),
                     [&allowed_items](const auto& item) {
                       return std::ranges::contains(allowed_items, item);
                     });
}

std::optional<std::string> GetThreadId(const GURL& url) {
  std::string value;
  if (url.is_valid() && url.has_query() &&
      net::GetValueForKeyInQuery(url, "mtid", &value)) {
    return value;
  }
  return std::nullopt;
}

}  // namespace

InputStateModel::InputStateModel(
    contextual_search::ContextualSearchSessionHandle& session_handle,
    const SearchboxConfig& config,
    const GURL& active_url,
    bool is_off_the_record,
    bool is_signed_in,
    bool browser_identity_matches_aim_identity)
    : session_handle_(session_handle.AsWeakPtr()),
      is_off_the_record_(is_off_the_record),
      is_signed_in_(is_signed_in),
      browser_identity_matches_aim_identity_(
          browser_identity_matches_aim_identity),
      current_url_(active_url) {
  PopulateConfig(config);

  state_.active_tool = omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
  state_.is_canvas_query_submitted = false;
  if (auto parsed_tool = GetActiveToolFromUrl(active_url, state_.tool_configs,
                                              state_.allowed_tools);
      parsed_tool.has_value()) {
    state_.active_tool = *parsed_tool;
    if (*parsed_tool == omnibox::ToolMode::TOOL_MODE_CANVAS) {
      state_.is_canvas_query_submitted = true;
    }
  }
  // The initial model should be the first allowed model, but can be
  // overridden by parameters in the active web contents URL.
  state_.active_model = state_.GetDefaultModel();

  if (auto parsed_model = GetActiveModelFromUrl(
          active_url, state_.model_configs, state_.allowed_models);
      parsed_model.has_value()) {
    state_.active_model = *parsed_model;
  }

  state_.image_gen_upload_active = false;

  updateDisabledState();
}

void InputStateModel::PopulateConfig(const SearchboxConfig& config) {
  has_valid_config_ = IsConfigPopulated(&config);
  serialized_config_ = config.SerializeAsString();

  SearchboxConfig mutable_config = config;
  MaybePopulateBrowserTabInputTypeRule(&mutable_config);
  MaybePopulateDriveInputTypeRule(&mutable_config);

  if (mutable_config.has_rule_set()) {
    rule_set_ = mutable_config.rule_set();

    state_.allowed_tools.clear();
    state_.allowed_tools.reserve(mutable_config.tool_configs().size());
    for (const auto& tool_config : mutable_config.tool_configs()) {
      if (tool_config.hide_from_menu()) {
        continue;
      }
      if (tool_config.tool() == omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD) {
        continue;
      }
      if (is_off_the_record_ &&
          tool_config.tool() == omnibox::ToolMode::TOOL_MODE_IMAGE_GEN) {
        continue;
      }
      state_.allowed_tools.push_back(tool_config.tool());
    }
    state_.allowed_models.clear();
    state_.allowed_models.reserve(mutable_config.model_configs().size());
    for (const auto& model_config : mutable_config.model_configs()) {
      state_.allowed_models.push_back(model_config.model());
    }
    configured_input_types_.clear();
    configured_input_types_.reserve(mutable_config.input_type_configs().size());
    for (const auto& input_type_config : mutable_config.input_type_configs()) {
      if (input_type_config.has_input_type()) {
        configured_input_types_.push_back(input_type_config.input_type());
      }
    }
    state_.tool_configs.clear();
    state_.tool_configs.reserve(mutable_config.tool_configs_size());
    for (const auto& tool_config : mutable_config.tool_configs()) {
      state_.tool_configs.push_back(tool_config);
    }
    state_.model_configs.clear();
    state_.model_configs.reserve(mutable_config.model_configs_size());
    for (const auto& model_config : mutable_config.model_configs()) {
      state_.model_configs.push_back(model_config);
    }
    state_.input_type_configs.clear();
    state_.input_type_configs.reserve(mutable_config.input_type_configs_size());
    for (const auto& input_type_config : mutable_config.input_type_configs()) {
      state_.input_type_configs.push_back(input_type_config);
    }
    if (mutable_config.has_tools_section_config()) {
      state_.tools_section_config = mutable_config.tools_section_config();
    } else {
      state_.tools_section_config.reset();
    }
    if (mutable_config.has_model_section_config()) {
      state_.model_section_config = mutable_config.model_section_config();
    } else {
      state_.model_section_config.reset();
    }
    if (mutable_config.has_hint_text()) {
      state_.hint_text = mutable_config.hint_text();
    } else {
      state_.hint_text.clear();
    }
    if (rule_set_.has_max_total_inputs()) {
      state_.max_total_inputs = rule_set_.max_total_inputs();
    } else {
      state_.max_total_inputs = 0;
    }
    state_.max_inputs_by_type.clear();
    for (const auto& rule : rule_set_.input_type_rules()) {
      if (rule.has_input_type() && rule.has_max_instance()) {
        state_.max_inputs_by_type[rule.input_type()] = rule.max_instance();
      }
    }
  }
}

bool InputStateModel::UpdateConfig(const SearchboxConfig& config) {
  if (!IsConfigPopulated(&config)) {
    return false;
  }
  std::string serialized = config.SerializeAsString();
  if (has_valid_config_ && serialized == serialized_config_) {
    return false;
  }

  PopulateConfig(config);

  if (!std::ranges::contains(state_.allowed_models, state_.active_model)) {
    state_.active_model = state_.GetDefaultModel();
  }
  if (state_.active_tool != omnibox::ToolMode::TOOL_MODE_UNSPECIFIED &&
      !std::ranges::contains(state_.allowed_tools, state_.active_tool)) {
    state_.active_tool = omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
  }

  updateDisabledState();
  RebuildAllowedInputTypes();

  subscribers_.Notify(state_);
  return true;
}

InputStateModel::InputStateModel(
    const InputStateModel& new_input_state_model,
    contextual_search::ContextualSearchSessionHandle& new_session_handle)
    : session_handle_(new_session_handle.AsWeakPtr()),
      is_off_the_record_(new_input_state_model.is_off_the_record_),
      is_signed_in_(new_input_state_model.is_signed_in_),
      browser_identity_matches_aim_identity_(
          new_input_state_model.browser_identity_matches_aim_identity_),
      current_url_(new_input_state_model.current_url_) {
  state_ = new_input_state_model.state_;
  rule_set_ = new_input_state_model.rule_set_;
  serialized_config_ = new_input_state_model.serialized_config_;
  configured_input_types_ = new_input_state_model.configured_input_types_;
  has_valid_config_ = new_input_state_model.has_valid_config_;
  is_smart_tab_sharing_active_ =
      new_input_state_model.is_smart_tab_sharing_active_;
  permanently_disabled_tools_ =
      new_input_state_model.permanently_disabled_tools_;
  permanently_disabled_input_types_ =
      new_input_state_model.permanently_disabled_input_types_;
  if (new_input_state_model.pref_service_) {
    SetPrefService(new_input_state_model.pref_service_);
  }
  user_modified_tool_in_thread_ =
      new_input_state_model.user_modified_tool_in_thread_;
}

InputStateModel::~InputStateModel() = default;

// static
bool InputStateModel::IsConfigPopulated(
    const omnibox::SearchboxConfig* config) {
  return config && (config->has_rule_set() || !config->tool_configs().empty() ||
                    !config->model_configs().empty() ||
                    !config->input_type_configs().empty());
}

// static
std::vector<omnibox::InputType> InputStateModel::GetCurrentInputTypes(
    const contextual_search::ContextualSearchSessionHandle* session_handle) {
  std::vector<omnibox::InputType> input_types;
  if (!session_handle) {
    return input_types;
  }
  const auto& uploaded_files = session_handle->GetUploadedContextFileInfos();
  input_types.reserve(uploaded_files.size());
  for (const auto& file_info : uploaded_files) {
    if (file_info.tab_url) {
      input_types.push_back(omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
      continue;
    }
    if (file_info.input_data && file_info.input_data->drive_id.has_value()) {
      input_types.push_back(omnibox::InputType::INPUT_TYPE_DRIVE);
      continue;
    }
    switch (file_info.mime_type) {
      case lens::MimeType::kImage:
        input_types.push_back(omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
        break;
      case lens::MimeType::kPdf:
        input_types.push_back(omnibox::InputType::INPUT_TYPE_LENS_FILE);
        break;
      default:
        input_types.push_back(omnibox::InputType::INPUT_TYPE_UNSPECIFIED);
        break;
    }
  }
  return input_types;
}

void InputStateModel::Initialize() {
  notifySubscribers();
}

void InputStateModel::SetSmartTabSharingActive(bool active) {
  if (is_smart_tab_sharing_active_ == active) {
    return;
  }
  is_smart_tab_sharing_active_ = active;
  updateDisabledState();
  notifySubscribers();
}

std::vector<omnibox::InputType> InputStateModel::GetEffectiveInputTypes()
    const {
  std::vector<omnibox::InputType> input_types =
      GetCurrentInputTypes(session_handle_.get());
  if (is_smart_tab_sharing_active_) {
    bool is_browser_tab_allowed = true;
    if (state_.active_tool != omnibox::ToolMode::TOOL_MODE_UNSPECIFIED) {
      const omnibox::ToolRule* rule = GetToolRule(state_.active_tool);
      if (!rule || (!rule->allow_all_input_types() &&
                    !std::ranges::contains(
                        rule->allowed_input_types(),
                        omnibox::InputType::INPUT_TYPE_BROWSER_TAB))) {
        is_browser_tab_allowed = false;
      }
    }
    if (state_.active_model != omnibox::ModelMode::MODEL_MODE_UNSPECIFIED) {
      const omnibox::ModelRule* rule = GetModelRule(state_.active_model);
      if (!rule || (!rule->allow_all_input_types() &&
                    !std::ranges::contains(
                        rule->allowed_input_types(),
                        omnibox::InputType::INPUT_TYPE_BROWSER_TAB))) {
        is_browser_tab_allowed = false;
      }
    }
    if (is_browser_tab_allowed &&
        !std::ranges::contains(input_types,
                               omnibox::InputType::INPUT_TYPE_BROWSER_TAB)) {
      input_types.push_back(omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
    }
  }
  return input_types;
}

void InputStateModel::SetPrefService(PrefService* pref_service) {
  pref_service_ = pref_service;
  pref_change_registrar_.Reset();
  if (pref_service_) {
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        contextual_search::kSearchContentSharingSettings,
        base::BindRepeating(&InputStateModel::OnPrefChanged,
                            base::Unretained(this)));
    OnPrefChanged();
  } else {
    updateDisabledState();
  }
}

void InputStateModel::OnPrefChanged() {
  if (!pref_service_) {
    return;
  }

  updateDisabledState();
  notifySubscribers();
}

base::CallbackListSubscription InputStateModel::subscribe(Subscriber callback) {
  return subscribers_.Add(std::move(callback));
}

void InputStateModel::notifySubscribers() {
  subscribers_.Notify(state_);
}

void InputStateModel::setActiveTool(ToolMode tool) {
  if (tool != state_.active_tool) {
    user_modified_tool_in_thread_ = true;
  }
  if (tool == omnibox::ToolMode::TOOL_MODE_UNSPECIFIED) {
    state_.is_canvas_query_submitted = false;
  }
  updateSelectedState(tool, state_.active_model);
}

void InputStateModel::setActiveModel(ModelMode model) {
  updateSelectedState(state_.active_tool, model);
}

void InputStateModel::UpdateStateFromUrl(const GURL& url) {
  auto matched_tool =
      GetActiveToolFromUrl(url, state_.tool_configs, state_.allowed_tools);

  auto prev_thread_id = GetThreadId(current_url_);
  auto new_thread_id = GetThreadId(url);

  bool thread_changed = prev_thread_id != new_thread_id;

  current_url_ = url;
  // If thread changes, be prepared to listen to any subsequent URL changes that
  // could include changes in the tool param (due to thread change).
  if (thread_changed) {
    user_modified_tool_in_thread_ = false;
  }

  ToolMode new_tool = state_.active_tool;

  // If the user has modified the tool in the thread, do not use tool from URL
  // params until user changes the thread, as that will dirty the tool state
  // with outdated tools that are only relevant at initialization.
  if (matched_tool.has_value() && !user_modified_tool_in_thread_) {
    new_tool = *matched_tool;
  } else if (thread_changed) {
    new_tool = ToolMode::TOOL_MODE_UNSPECIFIED;
  }

  auto matched_model =
      GetActiveModelFromUrl(url, state_.model_configs, state_.allowed_models);
  ModelMode new_model = matched_model.value_or(state_.active_model);

  state_.is_canvas_query_submitted =
      (new_tool == omnibox::ToolMode::TOOL_MODE_CANVAS);

  if (thread_changed || new_model != state_.active_model ||
      new_tool != state_.active_tool) {
    updateSelectedState(new_tool, new_model);
  }
}

void InputStateModel::OnContextChanged() {
  // Update the disabled state based on the new inputs uploaded.
  updateDisabledState();

  if (state_.active_tool == omnibox::ToolMode::TOOL_MODE_IMAGE_GEN) {
    const auto current_inputs = GetCurrentInputTypes(session_handle_.get());
    if (std::find(current_inputs.begin(), current_inputs.end(),
                  omnibox::InputType::INPUT_TYPE_LENS_IMAGE) ==
        current_inputs.end()) {
      state_.image_gen_upload_active = false;
    } else {
      state_.image_gen_upload_active = true;
    }
  }

  // Notify subscribers once `state_` is updated.
  notifySubscribers();
}

void InputStateModel::SetPermanentlyDisabledTools(
    const std::vector<ToolMode>& tools) {
  permanently_disabled_tools_ = tools;
  updateDisabledState();
  notifySubscribers();
}

void InputStateModel::SetPermanentlyDisabledInputTypes(
    const std::vector<InputType>& input_types) {
  permanently_disabled_input_types_ = input_types;
  updateDisabledState();
  notifySubscribers();
}

void InputStateModel::TogglePermanentlyDisabledInputType(InputType input_type,
                                                         bool disabled) {
  if (disabled) {
    if (!std::ranges::contains(permanently_disabled_input_types_, input_type)) {
      permanently_disabled_input_types_.push_back(input_type);
    }
  } else {
    std::erase(permanently_disabled_input_types_, input_type);
  }
  updateDisabledState();
  notifySubscribers();
}

void InputStateModel::updateSelectedState(ToolMode tool, ModelMode model) {
  state_.active_model = model;
  state_.image_gen_upload_active = false;

  // Set `image_gen_upload_active` to true if the active tool is
  // `TOOL_MODE_IMAGE_GEN` and an image is uploaded.
  if (tool == omnibox::ToolMode::TOOL_MODE_IMAGE_GEN) {
    const auto current_inputs = GetCurrentInputTypes(session_handle_.get());
    if (std::find(current_inputs.begin(), current_inputs.end(),
                  omnibox::InputType::INPUT_TYPE_LENS_IMAGE) !=
        current_inputs.end()) {
      state_.image_gen_upload_active = true;
    }
  }
  state_.active_tool = tool;

  // Update the disabled state based on the active model, tool, and current
  // input types.
  updateDisabledState();

  // Notify subscribers once `state_` is updated.
  notifySubscribers();
}

bool InputStateModel::IsDriveSupported() const {
  bool incognito = is_off_the_record_;
  bool feature_enabled =
      base::FeatureList::IsEnabled(omnibox::kComposeboxDriveContextMenuOption);
  bool identity_matches = browser_identity_matches_aim_identity_;

  // TODO(545561312): When restricted, the option should still be visible but
  // greyed out (disabled).

  if (incognito || !feature_enabled) {
    return false;
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Drive option is available even on signout with the signin promo.
  if (!is_signed_in_ &&
      base::FeatureList::IsEnabled(
          omnibox::kComposeboxDriveContextMenuOptionSigninPromo)) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  if (!is_signed_in_ || !identity_matches) {
    return false;
  }

  return true;
}

// Helper to check if search content sharing is enabled based on the
// user preference.
bool InputStateModel::IsSearchContentSharingEnabled() const {
  if (!pref_service_) {
    // Default behavior: if no `PrefService` default to allowed.
    return true;
  }

  // Read the pref value.
  int value = pref_service_->GetInteger(
      contextual_search::kSearchContentSharingSettings);

  // Comparison logic: must cast the enum class to an int for comparison.
  return value ==
         static_cast<int>(
             contextual_search::SearchContentSharingSettingsValue::kEnabled);
}

const omnibox::ModelRule* InputStateModel::GetModelRule(ModelMode model) const {
  auto it = std::find_if(state_.model_configs.begin(),
                         state_.model_configs.end(), [&](const auto& config) {
                           return config.model() == model && config.has_rule();
                         });

  if (it != state_.model_configs.end()) {
    return &it->rule();
  }
  return nullptr;
}

const omnibox::ToolRule* InputStateModel::GetToolRule(ToolMode tool) const {
  auto it = std::find_if(state_.tool_configs.begin(), state_.tool_configs.end(),
                         [&](const auto& config) {
                           return config.tool() == tool && config.has_rule();
                         });

  if (it != state_.tool_configs.end()) {
    return &it->rule();
  }
  return nullptr;
}

void InputStateModel::UpdateDisabledTools() {
  // Disable a tool if:
  // - It is currently active (to prevent re-activation).
  // - Incompatible with the active model.
  // - Incompatible with the current inputs.
  state_.disabled_tools.clear();
  state_.disabled_tools.reserve(state_.allowed_tools.size());
  const omnibox::ModelRule* active_model_rule =
      GetModelRule(state_.active_model);
  const auto effective_inputs = GetEffectiveInputTypes();
  for (const auto& tool : state_.allowed_tools) {
    if (tool == state_.active_tool) {
      state_.disabled_tools.push_back(tool);
      continue;
    }

    bool incompatible_with_model =
        state_.active_model != omnibox::ModelMode::MODEL_MODE_UNSPECIFIED &&
        active_model_rule && !active_model_rule->allow_all_tools() &&
        !std::ranges::contains(active_model_rule->allowed_tools(), tool);

    const omnibox::ToolRule* tool_rule = GetToolRule(tool);
    bool incompatible_with_inputs =
        !tool_rule ||
        (!tool_rule->allow_all_input_types() &&
         !AreItemsAllowed(effective_inputs, tool_rule->allowed_input_types()));

    if (incompatible_with_model || incompatible_with_inputs ||
        std::ranges::contains(permanently_disabled_tools_, tool)) {
      state_.disabled_tools.push_back(tool);
    }
  }
}

void InputStateModel::UpdateDisabledModels() {
  // Disable a model if:
  // - Incompatible with the active tool.
  // - Incompatible with the current inputs.
  state_.disabled_models.clear();
  state_.disabled_models.reserve(state_.allowed_models.size());
  const auto effective_inputs = GetEffectiveInputTypes();
  for (const auto& model : state_.allowed_models) {
    if (model == state_.active_model) {
      continue;
    }

    const omnibox::ModelRule* model_rule = GetModelRule(model);

    bool incompatible_with_tool =
        state_.active_tool != omnibox::ToolMode::TOOL_MODE_UNSPECIFIED &&
        (!model_rule || (!model_rule->allow_all_tools() &&
                         !std::ranges::contains(model_rule->allowed_tools(),
                                                state_.active_tool)));

    bool incompatible_with_inputs =
        (!model_rule || (!model_rule->allow_all_input_types() &&
                         !AreItemsAllowed(effective_inputs,
                                          model_rule->allowed_input_types())));

    if (incompatible_with_tool || incompatible_with_inputs) {
      state_.disabled_models.push_back(model);
    }
  }
}

void InputStateModel::UpdateDisabledInputTypes() {
  // Disable an input type if:
  // - Input type limit is reached.
  // - Total input limit is reached.
  // - Incompatible with the active model.
  // - Incompatible with the active tool.
  state_.disabled_input_types.clear();
  state_.disabled_input_types.reserve(state_.allowed_input_types.size());

  const auto current_inputs = GetEffectiveInputTypes();

  // Check max inputs reached.
  bool global_limit_reached =
      state_.max_total_inputs > 0 &&
      current_inputs.size() >= static_cast<size_t>(state_.max_total_inputs);

  if (global_limit_reached) {
    state_.disabled_input_types = state_.allowed_input_types;
    return;
  }

  const auto& limits = state_.max_inputs_by_type;
  std::map<omnibox::InputType, int> current_input_counts;
  for (const auto& input_type : current_inputs) {
    current_input_counts[input_type]++;
  }

  const omnibox::ModelRule* active_model_rule =
      GetModelRule(state_.active_model);
  const omnibox::ToolRule* active_tool_rule = GetToolRule(state_.active_tool);

  for (const auto& input_type : state_.allowed_input_types) {
    bool input_limit_reached = false;
    if (auto limits_it = limits.find(input_type); limits_it != limits.end()) {
      int limit = limits_it->second;
      if (limit > 0) {
        if (auto it = current_input_counts.find(input_type);
            it != current_input_counts.end() && it->second >= limit) {
          input_limit_reached = true;
        }
      }
    }

    bool incompatible_with_model =
        state_.active_model != omnibox::ModelMode::MODEL_MODE_UNSPECIFIED &&
        active_model_rule && !active_model_rule->allow_all_input_types() &&
        !std::ranges::contains(active_model_rule->allowed_input_types(),
                               input_type);

    bool incompatible_with_tool =
        state_.active_tool != omnibox::ToolMode::TOOL_MODE_UNSPECIFIED &&
        active_tool_rule && !active_tool_rule->allow_all_input_types() &&
        !std::ranges::contains(active_tool_rule->allowed_input_types(),
                               input_type);

    const bool should_disable_due_to_limit = input_limit_reached;

    if (should_disable_due_to_limit || incompatible_with_model ||
        incompatible_with_tool ||
        std::ranges::contains(permanently_disabled_input_types_, input_type)) {
      state_.disabled_input_types.push_back(input_type);
    }
  }
}

void InputStateModel::updateDisabledState() {
  RebuildAllowedInputTypes();
  UpdateDisabledTools();
  UpdateDisabledModels();
  UpdateDisabledInputTypes();
}

void InputStateModel::RebuildAllowedInputTypes() {
  state_.allowed_input_types.clear();
  bool sharing_enabled = IsSearchContentSharingEnabled();
  for (auto type : configured_input_types_) {
    if (!sharing_enabled) {
      if (type == omnibox::InputType::INPUT_TYPE_LENS_IMAGE ||
          type == omnibox::InputType::INPUT_TYPE_LENS_FILE ||
          type == omnibox::InputType::INPUT_TYPE_BROWSER_TAB ||
          type == omnibox::InputType::INPUT_TYPE_DRIVE) {
        continue;
      }
    }
    if (type == omnibox::InputType::INPUT_TYPE_DRIVE && !IsDriveSupported()) {
      continue;
    }
    state_.allowed_input_types.push_back(type);
  }

  auto contains = [&](omnibox::InputType type) {
    return std::find(state_.allowed_input_types.begin(),
                     state_.allowed_input_types.end(),
                     type) != state_.allowed_input_types.end();
  };

  // Fallback for browser tab if not already present and lens is allowed.
  if (!contains(omnibox::INPUT_TYPE_BROWSER_TAB) &&
      contains(omnibox::INPUT_TYPE_LENS_IMAGE) &&
      contains(omnibox::INPUT_TYPE_LENS_FILE) && sharing_enabled) {
    state_.allowed_input_types.push_back(omnibox::INPUT_TYPE_BROWSER_TAB);
  }

  // Fallback for drive if not already present in SearchboxConfig and drive is
  // supported. This option is available even on signout when the signin promo
  // feature flag is enabled, which will prompt the signin promo when clicked.
  if (!contains(omnibox::INPUT_TYPE_DRIVE) && IsDriveSupported() &&
      sharing_enabled) {
    state_.allowed_input_types.push_back(omnibox::INPUT_TYPE_DRIVE);
  }
}

std::map<std::string, std::string> InputStateModel::GetAdditionalQueryParams() {
  std::map<std::string, std::string> additional_params;
  if (state_.active_tool != omnibox::ToolMode::TOOL_MODE_UNSPECIFIED) {
    const auto tool_it =
        std::find_if(state_.tool_configs.begin(), state_.tool_configs.end(),
                     [&](const omnibox::ToolConfig& config) {
                       return config.tool() == state_.active_tool;
                     });
    if (tool_it != state_.tool_configs.end()) {
      for (const auto& param : tool_it->aim_url_params()) {
        additional_params[param.param_key()] = param.param_value();
      }
    }
  }
  if (state_.active_model != omnibox::ModelMode::MODEL_MODE_UNSPECIFIED) {
    const auto model_it =
        std::find_if(state_.model_configs.begin(), state_.model_configs.end(),
                     [&](const omnibox::ModelConfig& config) {
                       return config.model() == state_.active_model;
                     });
    if (model_it != state_.model_configs.end()) {
      for (const auto& param : model_it->aim_url_params()) {
        additional_params[param.param_key()] = param.param_value();
      }
    }
  } else {
    // If no model is selected, add a default param to indicate that the query
    // is an AIM query.
    additional_params["udm"] = "50";
  }
  return additional_params;
}

const InputState& InputStateModel::GetInputState() const {
  return state_;
}

}  // namespace contextual_search
