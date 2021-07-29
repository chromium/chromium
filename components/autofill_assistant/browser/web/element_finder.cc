// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/element_finder.h"

#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

namespace {
// Javascript code to get document root element.
const char kGetDocumentElement[] =
    "(function() { return document.documentElement; }())";

const char kGetArrayElement[] = "function(index) { return this[index]; }";

bool ConvertPseudoType(const PseudoType pseudo_type,
                       dom::PseudoType* pseudo_type_output) {
  switch (pseudo_type) {
    case PseudoType::UNDEFINED:
      break;
    case PseudoType::FIRST_LINE:
      *pseudo_type_output = dom::PseudoType::FIRST_LINE;
      return true;
    case PseudoType::FIRST_LETTER:
      *pseudo_type_output = dom::PseudoType::FIRST_LETTER;
      return true;
    case PseudoType::BEFORE:
      *pseudo_type_output = dom::PseudoType::BEFORE;
      return true;
    case PseudoType::AFTER:
      *pseudo_type_output = dom::PseudoType::AFTER;
      return true;
    case PseudoType::BACKDROP:
      *pseudo_type_output = dom::PseudoType::BACKDROP;
      return true;
    case PseudoType::SELECTION:
      *pseudo_type_output = dom::PseudoType::SELECTION;
      return true;
    case PseudoType::FIRST_LINE_INHERITED:
      *pseudo_type_output = dom::PseudoType::FIRST_LINE_INHERITED;
      return true;
    case PseudoType::SCROLLBAR:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR;
      return true;
    case PseudoType::SCROLLBAR_THUMB:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_THUMB;
      return true;
    case PseudoType::SCROLLBAR_BUTTON:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_BUTTON;
      return true;
    case PseudoType::SCROLLBAR_TRACK:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_TRACK;
      return true;
    case PseudoType::SCROLLBAR_TRACK_PIECE:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_TRACK_PIECE;
      return true;
    case PseudoType::SCROLLBAR_CORNER:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_CORNER;
      return true;
    case PseudoType::RESIZER:
      *pseudo_type_output = dom::PseudoType::RESIZER;
      return true;
    case PseudoType::INPUT_LIST_BUTTON:
      *pseudo_type_output = dom::PseudoType::INPUT_LIST_BUTTON;
      return true;
  }
  return false;
}

ClientStatus MoveAutofillValueRegexpToTextFilter(
    const UserData* user_data,
    SelectorProto::PropertyFilter* value) {
  if (!value->has_autofill_value_regexp()) {
    return OkClientStatus();
  }
  if (user_data == nullptr) {
    return ClientStatus(PRECONDITION_FAILED);
  }
  const AutofillValueRegexp& autofill_value_regexp =
      value->autofill_value_regexp();
  TextFilter text_filter;
  text_filter.set_case_sensitive(
      autofill_value_regexp.value_expression_re2().case_sensitive());
  std::string re2;
  ClientStatus re2_status = user_data::GetFormattedClientValue(
      autofill_value_regexp, user_data, &re2);
  text_filter.set_re2(re2);
  // Assigning text_filter will clear autofill_value_regexp.
  *value->mutable_text_filter() = text_filter;
  return re2_status;
}

ClientStatus GetUserDataResolvedSelector(const Selector& selector,
                                         const UserData* user_data,
                                         SelectorProto* out_selector) {
  SelectorProto copy = selector.proto;
  for (auto& filter : *copy.mutable_filters()) {
    switch (filter.filter_case()) {
      case SelectorProto::Filter::kProperty: {
        ClientStatus filter_status = MoveAutofillValueRegexpToTextFilter(
            user_data, filter.mutable_property());
        if (!filter_status.ok()) {
          return filter_status;
        }
        break;
      }
      case SelectorProto::Filter::kInnerText:
      case SelectorProto::Filter::kValue:
      case SelectorProto::Filter::kPseudoElementContent:
      case SelectorProto::Filter::kCssStyle:
      case SelectorProto::Filter::kCssSelector:
      case SelectorProto::Filter::kEnterFrame:
      case SelectorProto::Filter::kPseudoType:
      case SelectorProto::Filter::kBoundingBox:
      case SelectorProto::Filter::kNthMatch:
      case SelectorProto::Filter::kLabelled:
      case SelectorProto::Filter::kMatchCssSelector:
      case SelectorProto::Filter::kOnTop:
      case SelectorProto::Filter::FILTER_NOT_SET:
        break;
        // Do not add default here. In case a new filter gets added (that may
        // contain a RegexpFilter) we want this to fail at compilation here.
    }
  }
  *out_selector = copy;
  return OkClientStatus();
}

}  // namespace

ElementFinder::JsFilterBuilder::JsFilterBuilder() = default;
ElementFinder::JsFilterBuilder::~JsFilterBuilder() = default;

std::vector<std::unique_ptr<runtime::CallArgument>>
ElementFinder::JsFilterBuilder::BuildArgumentList() const {
  auto str_array_arg = std::make_unique<base::Value>(base::Value::Type::LIST);
  for (const std::string& str : arguments_) {
    str_array_arg->Append(str);
  }
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  arguments.emplace_back(runtime::CallArgument::Builder()
                             .SetValue(std::move(str_array_arg))
                             .Build());
  return arguments;
}

// clang-format off
std::string ElementFinder::JsFilterBuilder::BuildFunction() const {
  return base::StrCat({
    R"(
    function(args) {
      let elements = [this];
    )",
    snippet_.ToString(),
    R"(
      if (elements.length == 0) return null;
      if (elements.length == 1) { return elements[0] }
      return elements;
    })"
  });
}
// clang-format on

bool ElementFinder::JsFilterBuilder::AddFilter(
    const SelectorProto::Filter& filter) {
  switch (filter.filter_case()) {
    case SelectorProto::Filter::kCssSelector:
      // We querySelectorAll the current elements and remove duplicates, which
      // are likely when using inner text before CSS selector filters. We must
      // not return duplicates as they cause incorrect TOO_MANY_ELEMENTS errors.
      DefineQueryAllDeduplicated();
      AddLine({"elements = queryAllDeduplicated(elements, ",
               AddArgument(filter.css_selector()), ");"});
      return true;

    case SelectorProto::Filter::kInnerText:
      AddRegexpFilter(filter.inner_text(), "innerText");
      return true;

    case SelectorProto::Filter::kValue:
      AddRegexpFilter(filter.value(), "value");
      return true;

    case SelectorProto::Filter::kProperty:
      AddRegexpFilter(filter.property().text_filter(),
                      filter.property().property());
      return true;

    case SelectorProto::Filter::kBoundingBox:
      if (filter.bounding_box().require_nonempty()) {
        AddLine("elements = elements.filter((e) => {");
        AddLine("  const rect = e.getBoundingClientRect();");
        AddLine("  return rect.width != 0 && rect.height != 0;");
        AddLine("});");
      } else {
        AddLine(
            "elements = elements.filter((e) => e.getClientRects().length > "
            "0);");
      }
      return true;

    case SelectorProto::Filter::kPseudoElementContent: {
      // When a content is set, window.getComputedStyle().content contains a
      // double-quoted string with the content, unquoted here by JSON.parse().
      std::string re_var =
          AddRegexpInstance(filter.pseudo_element_content().content());
      std::string pseudo_type =
          PseudoTypeName(filter.pseudo_element_content().pseudo_type());

      AddLine("elements = elements.filter((e) => {");
      AddLine({"  const s = window.getComputedStyle(e, '", pseudo_type, "');"});
      AddLine("  if (!s || !s.content || !s.content.startsWith('\"')) {");
      AddLine("    return false;");
      AddLine("  }");
      AddLine({"  return ", re_var, ".test(JSON.parse(s.content));"});
      AddLine("});");
      return true;
    }

    case SelectorProto::Filter::kCssStyle: {
      std::string re_var = AddRegexpInstance(filter.css_style().value());
      std::string property = AddArgument(filter.css_style().property());
      std::string element = AddArgument(filter.css_style().pseudo_element());
      AddLine("elements = elements.filter((e) => {");
      AddLine("  const s = window.getComputedStyle(e, ");
      AddLine({"      ", element, " === '' ? null : ", element, ");"});
      AddLine({"  const match = ", re_var, ".test(s[", property, "]);"});
      if (filter.css_style().should_match()) {
        AddLine("  return match;");
      } else {
        AddLine("  return !match;");
      }
      AddLine("});");
      return true;
    }

    case SelectorProto::Filter::kLabelled:
      AddLine("elements = elements.flatMap((e) => {");
      AddLine(
          "  return e.tagName === 'LABEL' && e.control ? [e.control] : [];");
      AddLine("});");
      return true;

    case SelectorProto::Filter::kMatchCssSelector:
      AddLine({"elements = elements.filter((e) => e.webkitMatchesSelector(",
               AddArgument(filter.match_css_selector()), "));"});
      return true;

    case SelectorProto::Filter::kOnTop:
      AddLine("elements = elements.filter((e) => {");
      AddLine("if (e.getClientRects().length == 0) return false;");
      if (filter.on_top().scroll_into_view_if_needed()) {
        AddLine("e.scrollIntoViewIfNeeded(false);");
      }
      AddReturnIfOnTop(
          &snippet_, "e", /* on_top= */ "true", /* not_on_top= */ "false",
          /* not_in_view= */ filter.on_top().accept_element_if_not_in_view()
              ? "true"
              : "false");
      AddLine("});");
      return true;

    case SelectorProto::Filter::kEnterFrame:
    case SelectorProto::Filter::kPseudoType:
    case SelectorProto::Filter::kNthMatch:
    case SelectorProto::Filter::FILTER_NOT_SET:
      return false;
  }
}

std::string ElementFinder::JsFilterBuilder::AddRegexpInstance(
    const TextFilter& filter) {
  std::string re_flags = filter.case_sensitive() ? "" : "i";
  std::string re_var = DeclareVariable();
  AddLine({"const ", re_var, " = RegExp(", AddArgument(filter.re2()), ", '",
           re_flags, "');"});
  return re_var;
}

void ElementFinder::JsFilterBuilder::AddRegexpFilter(
    const TextFilter& filter,
    const std::string& property) {
  std::string re_var = AddRegexpInstance(filter);
  AddLine({"elements = elements.filter((e) => ", re_var, ".test(e.", property,
           "));"});
}

std::string ElementFinder::JsFilterBuilder::DeclareVariable() {
  return base::StrCat({"v", base::NumberToString(variable_counter_++)});
}

std::string ElementFinder::JsFilterBuilder::AddArgument(
    const std::string& value) {
  int index = arguments_.size();
  arguments_.emplace_back(value);
  return base::StrCat({"args[", base::NumberToString(index), "]"});
}

void ElementFinder::JsFilterBuilder::DefineQueryAllDeduplicated() {
  // Ensure that we don't define the function more than once.
  if (defined_query_all_deduplicated_)
    return;

  defined_query_all_deduplicated_ = true;

  AddLine(R"(
    const queryAllDeduplicated = function(roots, selector) {
      if (roots.length == 0) {
        return [];
      }

      const matchesSet = new Set();
      const matches = [];
      roots.forEach((root) => {
        root.querySelectorAll(selector).forEach((elem) => {
          if (!matchesSet.has(elem)) {
            matchesSet.add(elem);
            matches.push(elem);
          }
        });
      });
      return matches;
    }
  )");
}

ElementFinder::Result::Result() = default;

ElementFinder::Result::~Result() = default;

ElementFinder::Result::Result(const Result&) = default;

ElementFinder::ElementFinder(content::WebContents* web_contents,
                             DevtoolsClient* devtools_client,
                             const UserData* user_data,
                             const Selector& selector,
                             ResultType result_type)
    : web_contents_(web_contents),
      devtools_client_(devtools_client),
      user_data_(user_data),
      selector_(selector),
      result_type_(result_type) {}

ElementFinder::~ElementFinder() = default;

void ElementFinder::Start(Callback callback) {
  StartInternal(std::move(callback), web_contents_->GetMainFrame(),
                /* frame_id= */ "", /* document_object_id= */ "");
}

void ElementFinder::StartInternal(Callback callback,
                                  content::RenderFrameHost* frame,
                                  const std::string& frame_id,
                                  const std::string& document_object_id) {
  callback_ = std::move(callback);

  if (selector_.empty()) {
    SendResult(ClientStatus(INVALID_SELECTOR));
    return;
  }

  ClientStatus resolve_status =
      GetUserDataResolvedSelector(selector_, user_data_, &selector_proto_);
  if (!resolve_status.ok()) {
    SendResult(resolve_status);
    return;
  }

  current_frame_ = frame;
  current_frame_id_ = frame_id;
  current_frame_root_ = document_object_id;
  if (current_frame_root_.empty()) {
    GetDocumentElement();
  } else {
    current_matches_.emplace_back(current_frame_root_);
    ExecuteNextTask();
  }
}

void ElementFinder::SendResult(const ClientStatus& status) {
  if (!callback_)
    return;

  std::move(callback_).Run(status, std::make_unique<Result>());
}

void ElementFinder::SendSuccessResult(const std::string& object_id) {
  if (!callback_)
    return;

  // Fill in result and return
  std::unique_ptr<Result> result =
      std::make_unique<Result>(BuildResult(object_id));
  result->dom_object.frame_stack = frame_stack_;
  std::move(callback_).Run(OkClientStatus(), std::move(result));
}

ElementFinder::Result ElementFinder::BuildResult(const std::string& object_id) {
  Result result;
  result.container_frame_host = current_frame_;
  result.dom_object.object_data.object_id = object_id;
  result.dom_object.object_data.node_frame_id = current_frame_id_;
  return result;
}

void ElementFinder::ExecuteNextTask() {
  const auto& filters = selector_proto_.filters();

  if (next_filter_index_ >= filters.size()) {
    std::string object_id;
    switch (result_type_) {
      case ResultType::kExactlyOneMatch:
        if (!ConsumeOneMatchOrFail(object_id)) {
          return;
        }
        break;

      case ResultType::kAnyMatch:
        if (!ConsumeMatchAtOrFail(0, object_id)) {
          return;
        }
        break;

      case ResultType::kMatchArray:
        if (!ConsumeMatchArrayOrFail(object_id)) {
          return;
        }
        break;
    }
    SendSuccessResult(object_id);
    return;
  }

  const auto& filter = filters.Get(next_filter_index_);
  switch (filter.filter_case()) {
    case SelectorProto::Filter::kEnterFrame: {
      std::string object_id;
      if (!ConsumeOneMatchOrFail(object_id))
        return;

      // The above fails if there is more than one frame. To preserve
      // backward-compatibility with the previous, lax behavior, callers must
      // add pick_one before enter_frame. TODO(b/155264465): allow searching in
      // more than one frame.
      next_filter_index_++;
      EnterFrame(object_id);
      return;
    }

    case SelectorProto::Filter::kPseudoType: {
      std::vector<std::string> matches;
      if (!ConsumeAllMatchesOrFail(matches))
        return;

      next_filter_index_++;
      matching_pseudo_elements_ = true;
      ResolvePseudoElement(filter.pseudo_type(), matches);
      return;
    }

    case SelectorProto::Filter::kNthMatch: {
      std::string object_id;
      if (!ConsumeMatchAtOrFail(filter.nth_match().index(), object_id))
        return;

      next_filter_index_++;
      current_matches_ = {object_id};
      ExecuteNextTask();
      return;
    }

    case SelectorProto::Filter::kCssSelector:
    case SelectorProto::Filter::kInnerText:
    case SelectorProto::Filter::kValue:
    case SelectorProto::Filter::kProperty:
    case SelectorProto::Filter::kBoundingBox:
    case SelectorProto::Filter::kPseudoElementContent:
    case SelectorProto::Filter::kMatchCssSelector:
    case SelectorProto::Filter::kCssStyle:
    case SelectorProto::Filter::kLabelled:
    case SelectorProto::Filter::kOnTop: {
      std::vector<std::string> matches;
      if (!ConsumeAllMatchesOrFail(matches))
        return;

      JsFilterBuilder js_filter;
      for (int i = next_filter_index_; i < filters.size(); i++) {
        if (!js_filter.AddFilter(filters.Get(i))) {
          break;
        }
        next_filter_index_++;
      }
      ApplyJsFilters(js_filter, matches);
      return;
    }

    case SelectorProto::Filter::FILTER_NOT_SET:
      VLOG(1) << __func__ << " Unset or unknown filter in " << filter << " in "
              << selector_;
      SendResult(ClientStatus(INVALID_SELECTOR));
      return;
  }
}

bool ElementFinder::ConsumeOneMatchOrFail(std::string& object_id_out) {
  if (current_matches_.size() > 1) {
    VLOG(1) << __func__ << " Got " << current_matches_.size() << " matches for "
            << selector_ << ", when only 1 was expected.";
    SendResult(ClientStatus(TOO_MANY_ELEMENTS));
    return false;
  }
  if (current_matches_.empty()) {
    SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return false;
  }

  object_id_out = current_matches_[0];
  current_matches_.clear();
  return true;
}

bool ElementFinder::ConsumeMatchAtOrFail(size_t index,
                                         std::string& object_id_out) {
  if (index < current_matches_.size()) {
    object_id_out = current_matches_[index];
    current_matches_.clear();
    return true;
  }

  SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
  return false;
}

bool ElementFinder::ConsumeAllMatchesOrFail(
    std::vector<std::string>& matches_out) {
  if (!current_matches_.empty()) {
    matches_out = std::move(current_matches_);
    current_matches_.clear();
    return true;
  }
  SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
  return false;
}

bool ElementFinder::ConsumeMatchArrayOrFail(std::string& array_object_id) {
  if (!current_matches_js_array_.empty()) {
    array_object_id = current_matches_js_array_;
    current_matches_js_array_.clear();
    return true;
  }

  if (current_matches_.empty()) {
    SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return false;
  }

  MoveMatchesToJSArrayRecursive(/* index= */ 0);
  return false;
}

void ElementFinder::MoveMatchesToJSArrayRecursive(size_t index) {
  if (index >= current_matches_.size()) {
    current_matches_.clear();
    ExecuteNextTask();
    return;
  }

  // Push the value at |current_matches_[index]| to |current_matches_js_array_|.
  std::string function;
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  if (index == 0) {
    // Create an array containing a single element.
    function = "function() { return [this]; }";
  } else {
    // Add an element to an existing array.
    function = "function(dest) { dest.push(this); }";
    AddRuntimeCallArgumentObjectId(current_matches_js_array_, &arguments);
  }

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(current_matches_[index])
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(function)
          .Build(),
      current_frame_id_,
      base::BindOnce(&ElementFinder::OnMoveMatchesToJSArrayRecursive,
                     weak_ptr_factory_.GetWeakPtr(), index));
}

void ElementFinder::OnMoveMatchesToJSArrayRecursive(
    size_t index,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << ": Failed to push value to JS array.";
    SendResult(status);
    return;
  }

  // We just created an array which contains the first element. We store its ID
  // in |current_matches_js_array_|.
  if (index == 0 &&
      !SafeGetObjectId(result->GetResult(), &current_matches_js_array_)) {
    VLOG(1) << __func__ << " Failed to get array ID.";
    SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }

  // Continue the recursion to push the other values into the array.
  MoveMatchesToJSArrayRecursive(index + 1);
}

void ElementFinder::GetDocumentElement() {
  devtools_client_->GetRuntime()->Evaluate(
      std::string(kGetDocumentElement), current_frame_id_,
      base::BindOnce(&ElementFinder::OnGetDocumentElement,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ElementFinder::OnGetDocumentElement(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << " Failed to get document root element.";
    SendResult(status);
    return;
  }
  std::string object_id;
  if (!SafeGetObjectId(result->GetResult(), &object_id)) {
    VLOG(1) << __func__ << " Failed to get document root element.";
    SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }

  current_frame_root_ = object_id;
  // Use the node as root for the rest of the evaluation.
  current_matches_.emplace_back(object_id);

  ExecuteNextTask();
}

void ElementFinder::ApplyJsFilters(const JsFilterBuilder& builder,
                                   const std::vector<std::string>& object_ids) {
  DCHECK(!object_ids.empty());  // Guaranteed by ExecuteNextTask()
  PrepareBatchTasks(object_ids.size());
  std::string function = builder.BuildFunction();
  for (size_t task_id = 0; task_id < object_ids.size(); task_id++) {
    devtools_client_->GetRuntime()->CallFunctionOn(
        runtime::CallFunctionOnParams::Builder()
            .SetObjectId(object_ids[task_id])
            .SetArguments(builder.BuildArgumentList())
            .SetFunctionDeclaration(function)
            .Build(),
        current_frame_id_,
        base::BindOnce(&ElementFinder::OnApplyJsFilters,
                       weak_ptr_factory_.GetWeakPtr(), task_id));
  }
}

void ElementFinder::OnApplyJsFilters(
    size_t task_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!result) {
    // It is possible for a document element to already exist, but not be
    // available yet to query because the document hasn't been loaded. This
    // results in OnQuerySelectorAll getting a nullptr result. For this specific
    // call, it is expected.
    VLOG(1) << __func__ << ": Context doesn't exist yet to query frame "
            << frame_stack_.size() << " of " << selector_;
    SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << ": Failed to query selector for frame "
            << frame_stack_.size() << " of " << selector_ << ": " << status;
    SendResult(status);
    return;
  }

  // The result can be empty (nothing found), an array (multiple matches
  // found) or a single node.
  std::string object_id;
  if (!SafeGetObjectId(result->GetResult(), &object_id)) {
    ReportNoMatchingElement(task_id);
    return;
  }

  if (result->GetResult()->HasSubtype() &&
      result->GetResult()->GetSubtype() ==
          runtime::RemoteObjectSubtype::ARRAY) {
    ReportMatchingElementsArray(task_id, object_id);
    return;
  }

  ReportMatchingElement(task_id, object_id);
}

void ElementFinder::ResolvePseudoElement(
    PseudoType proto_pseudo_type,
    const std::vector<std::string>& object_ids) {
  dom::PseudoType pseudo_type;
  if (!ConvertPseudoType(proto_pseudo_type, &pseudo_type)) {
    VLOG(1) << __func__ << ": Unsupported pseudo-type "
            << PseudoTypeName(proto_pseudo_type);
    SendResult(ClientStatus(INVALID_ACTION));
    return;
  }

  DCHECK(!object_ids.empty());  // Guaranteed by ExecuteNextTask()
  PrepareBatchTasks(object_ids.size());
  for (size_t task_id = 0; task_id < object_ids.size(); task_id++) {
    devtools_client_->GetDOM()->DescribeNode(
        dom::DescribeNodeParams::Builder()
            .SetObjectId(object_ids[task_id])
            .Build(),
        current_frame_id_,
        base::BindOnce(&ElementFinder::OnDescribeNodeForPseudoElement,
                       weak_ptr_factory_.GetWeakPtr(), pseudo_type, task_id));
  }
}

void ElementFinder::OnDescribeNodeForPseudoElement(
    dom::PseudoType pseudo_type,
    size_t task_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    VLOG(1) << __func__ << " Failed to describe the node for pseudo element.";
    SendResult(UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  auto* node = result->GetNode();
  if (node->HasPseudoElements()) {
    for (const auto& pseudo_element : *(node->GetPseudoElements())) {
      if (pseudo_element->HasPseudoType() &&
          pseudo_element->GetPseudoType() == pseudo_type) {
        devtools_client_->GetDOM()->ResolveNode(
            dom::ResolveNodeParams::Builder()
                .SetBackendNodeId(pseudo_element->GetBackendNodeId())
                .Build(),
            current_frame_id_,
            base::BindOnce(&ElementFinder::OnResolveNodeForPseudoElement,
                           weak_ptr_factory_.GetWeakPtr(), task_id));
        return;
      }
    }
  }

  ReportNoMatchingElement(task_id);
}

void ElementFinder::OnResolveNodeForPseudoElement(
    size_t task_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (result && result->GetObject() && result->GetObject()->HasObjectId()) {
    ReportMatchingElement(task_id, result->GetObject()->GetObjectId());
    return;
  }

  ReportNoMatchingElement(task_id);
}

void ElementFinder::EnterFrame(const std::string& object_id) {
  devtools_client_->GetDOM()->DescribeNode(
      dom::DescribeNodeParams::Builder().SetObjectId(object_id).Build(),
      current_frame_id_,
      base::BindOnce(&ElementFinder::OnDescribeNodeForFrame,
                     weak_ptr_factory_.GetWeakPtr(), object_id));
}

void ElementFinder::OnDescribeNodeForFrame(
    const std::string& object_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    VLOG(1) << __func__ << " Failed to describe the node.";
    SendResult(UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  auto* node = result->GetNode();
  std::vector<int> backend_ids;

  if (node->GetNodeName() == "IFRAME") {
    DCHECK(node->HasFrameId());  // Ensure all frames have an id.

    frame_stack_.push_back({object_id, current_frame_id_});

    auto* frame =
        FindCorrespondingRenderFrameHost(node->GetFrameId(), web_contents_);
    if (!frame) {
      VLOG(1) << __func__ << " Failed to find corresponding owner frame.";
      SendResult(ClientStatus(FRAME_HOST_NOT_FOUND));
      return;
    }
    current_frame_ = frame;
    current_frame_root_.clear();

    if (node->HasContentDocument()) {
      // If the frame has a ContentDocument it's considered a local frame. In
      // this case, current_frame_ doesn't change and can directly use the
      // content document as root for the evaluation.
      backend_ids.emplace_back(node->GetContentDocument()->GetBackendNodeId());
    } else {
      current_frame_id_ = node->GetFrameId();
      // Kick off another find element chain to walk down the OOP iFrame.
      GetDocumentElement();
      return;
    }
  }

  if (node->HasShadowRoots()) {
    // TODO(crbug.com/806868): Support multiple shadow roots.
    backend_ids.emplace_back(
        node->GetShadowRoots()->front()->GetBackendNodeId());
  }

  if (!backend_ids.empty()) {
    devtools_client_->GetDOM()->ResolveNode(
        dom::ResolveNodeParams::Builder()
            .SetBackendNodeId(backend_ids[0])
            .Build(),
        current_frame_id_,
        base::BindOnce(&ElementFinder::OnResolveNode,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Element was not a frame and didn't have shadow dom. This is unexpected, but
  // to remain backward compatible, don't complain and just continue filtering
  // with the current element as root.
  current_matches_.emplace_back(object_id);
  ExecuteNextTask();
}

void ElementFinder::OnResolveNode(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (!result || !result->GetObject() || !result->GetObject()->HasObjectId()) {
    VLOG(1) << __func__ << " Failed to resolve object id from backend id.";
    SendResult(UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  std::string object_id = result->GetObject()->GetObjectId();
  if (current_frame_root_.empty()) {
    current_frame_root_ = object_id;
  }
  // Use the node as root for the rest of the evaluation.
  current_matches_.emplace_back(object_id);
  ExecuteNextTask();
}

void ElementFinder::PrepareBatchTasks(int n) {
  tasks_results_.clear();
  tasks_results_.resize(n);
}

void ElementFinder::ReportMatchingElement(size_t task_id,
                                          const std::string& object_id) {
  tasks_results_[task_id] =
      std::make_unique<std::vector<std::string>>(1, object_id);
  MaybeFinalizeBatchTasks();
}

void ElementFinder::ReportNoMatchingElement(size_t task_id) {
  tasks_results_[task_id] = std::make_unique<std::vector<std::string>>();
  MaybeFinalizeBatchTasks();
}

void ElementFinder::ReportMatchingElementsArray(
    size_t task_id,
    const std::string& array_object_id) {
  // Recursively add each element ID to a vector then report it as this task
  // result.
  ReportMatchingElementsArrayRecursive(
      task_id, array_object_id, std::make_unique<std::vector<std::string>>(),
      /* index= */ 0);
}

void ElementFinder::ReportMatchingElementsArrayRecursive(
    size_t task_id,
    const std::string& array_object_id,
    std::unique_ptr<std::vector<std::string>> acc,
    int index) {
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(index, &arguments);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(array_object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kGetArrayElement))
          .Build(),
      current_frame_id_,
      base::BindOnce(&ElementFinder::OnReportMatchingElementsArrayRecursive,
                     weak_ptr_factory_.GetWeakPtr(), task_id, array_object_id,
                     std::move(acc), index));
}

void ElementFinder::OnReportMatchingElementsArrayRecursive(
    size_t task_id,
    const std::string& array_object_id,
    std::unique_ptr<std::vector<std::string>> acc,
    int index,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << ": Failed to get element from array for "
            << selector_;
    SendResult(status);
    return;
  }

  std::string object_id;
  if (!SafeGetObjectId(result->GetResult(), &object_id)) {
    // We've reached the end of the array.
    tasks_results_[task_id] = std::move(acc);
    MaybeFinalizeBatchTasks();
    return;
  }

  acc->emplace_back(object_id);

  // Fetch the next element.
  ReportMatchingElementsArrayRecursive(task_id, array_object_id, std::move(acc),
                                       index + 1);
}

void ElementFinder::MaybeFinalizeBatchTasks() {
  // Return early if one of the tasks is still pending.
  for (const auto& result : tasks_results_) {
    if (!result) {
      return;
    }
  }

  // Add all matching elements to current_matches_.
  for (const auto& result : tasks_results_) {
    current_matches_.insert(current_matches_.end(), result->begin(),
                            result->end());
  }
  tasks_results_.clear();

  ExecuteNextTask();
}

}  // namespace autofill_assistant
