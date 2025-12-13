// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/controlled_frame/web_url_pattern_natives.h"

#include "base/containers/contains.h"
#include "base/containers/to_value_list.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/web/web_url_pattern_to_safe_url_pattern.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-isolate.h"

namespace controlled_frame {

namespace {

// This is for when there's no path, it gets inferred as wildcard,
// but also comes with a match group name equal to "0" that we need to ignore.
constexpr std::string kInferredWildcardPartName = "0";
constexpr const char* kAllowedProtocols[] = {"http", "https", "ws", "wss"};

bool WouldBeValidMatchPattern(const blink::SafeUrlPattern& safe_url_pattern) {
  // Protocol: one part, either wildcard or fixed in allowed set
  if (safe_url_pattern.protocol.size() != 1) {
    return false;
  }
  const liburlpattern::Part& protocol_part = safe_url_pattern.protocol[0];
  const bool protocol_is_wildcard =
      protocol_part.type == liburlpattern::PartType::kFullWildcard;
  const bool protocol_is_fixed =
      protocol_part.type == liburlpattern::PartType::kFixed;
  const bool protocol_allowed =
      protocol_is_fixed &&
      base::Contains(kAllowedProtocols, protocol_part.value);
  if (!protocol_is_wildcard && !protocol_allowed) {
    return false;
  }
  if (protocol_part.modifier != liburlpattern::Modifier::kNone) {
    return false;
  }

  // Hostname: first part must be wildcard or fixed, rest must be fixed
  const std::vector<liburlpattern::Part>& host_parts =
      safe_url_pattern.hostname;
  if (host_parts.empty()) {
    return false;
  }
  if (host_parts[0].type != liburlpattern::PartType::kFullWildcard &&
      host_parts[0].type != liburlpattern::PartType::kFixed) {
    return false;
  }
  for (size_t i = 1; i < host_parts.size(); ++i) {
    const bool fixed_and_starts_with_dot =
        host_parts[i].type == liburlpattern::PartType::kFixed &&
        host_parts[i].value[0] == '.';
    if (!fixed_and_starts_with_dot) {
      return false;
    }
  }
  for (const liburlpattern::Part& host_part : host_parts) {
    if (host_part.modifier != liburlpattern::Modifier::kNone) {
      return false;
    }
  }

  // Port: must be wildcard or fixed
  const std::vector<liburlpattern::Part>& port_parts = safe_url_pattern.port;
  if (!port_parts.empty() &&
      port_parts[0].type != liburlpattern::PartType::kFixed &&
      port_parts[0].type != liburlpattern::PartType::kFullWildcard) {
    return false;
  } else if (port_parts.size() > 1) {
    return false;
    // Protocol cannot be wildcard if port is fixed
  } else if (!port_parts.empty() &&
             port_parts[0].type == liburlpattern::PartType::kFixed &&
             protocol_is_wildcard) {
    return false;
  } else if (!port_parts.empty() &&
             port_parts[0].modifier != liburlpattern::Modifier::kNone) {
    return false;
  }

  // Don't allow match groups.
  for (const liburlpattern::Part& path_part : safe_url_pattern.pathname) {
    if (!path_part.name.empty() &&
        path_part.name != kInferredWildcardPartName) {
      return false;
    } else if (path_part.modifier != liburlpattern::Modifier::kNone) {
      return false;
    }
  }

  // Username, password: must be empty
  auto part_is_empty = [](const std::vector<liburlpattern::Part>& parts) {
    return parts.empty() || parts[0].value.empty() ||
           parts[0].type == liburlpattern::PartType::kFullWildcard;
  };
  if (!part_is_empty(safe_url_pattern.username) ||
      !part_is_empty(safe_url_pattern.password)) {
    return false;
  }
  return true;
}

std::vector<std::string> MatchPatternsFromUrlPattern(
    const blink::SafeUrlPattern& safe_url_pattern) {
  std::vector<std::string> match_patterns;

  auto append_part = [](std::string& out, const liburlpattern::Part& part) {
    if (part.type == liburlpattern::PartType::kFullWildcard) {
      out += part.prefix + "*" + part.suffix;
    } else {
      out += part.value;
    }
  };

  std::string protocol_host_str;
  const liburlpattern::Part& protocol_part = safe_url_pattern.protocol[0];
  append_part(protocol_host_str, protocol_part);

  protocol_host_str += "://";

  for (const liburlpattern::Part& host_part : safe_url_pattern.hostname) {
    append_part(protocol_host_str, host_part);
  }

  if (!safe_url_pattern.port.empty()) {
    protocol_host_str += ":";
    append_part(protocol_host_str, safe_url_pattern.port[0]);
  }

  std::string path_str;
  for (const liburlpattern::Part& path_part : safe_url_pattern.pathname) {
    append_part(path_str, path_part);
  }

  if (!path_str.starts_with("/")) {
    path_str = "/" + path_str;
  }

  // If empty search params or wildcard search params,
  // add a match pattern without search params,
  // because match patterns with "?*" don't match empty search params.
  if (safe_url_pattern.search.empty() ||
      safe_url_pattern.search[0].type ==
          liburlpattern::PartType::kFullWildcard) {
    match_patterns.push_back(protocol_host_str + path_str);
  }

  // If not empty search params, add them as a separate match pattern.
  if (!safe_url_pattern.search.empty()) {
    std::string search_query_str = "?";
    for (const liburlpattern::Part& search_part : safe_url_pattern.search) {
      append_part(search_query_str, search_part);
    }
    match_patterns.push_back(protocol_host_str + path_str + search_query_str);
  }

  // If "*" protocol, supply "ws" and "wss" as matching protocols as well.
  if (protocol_part.type == liburlpattern::PartType::kFullWildcard) {
    std::vector<std::string> ws_wss_additional_patterns;
    for (std::string& match_pattern : match_patterns) {
      CHECK_EQ(match_pattern[0], '*');
      std::string pattern_without_wildcard_protocol = match_pattern.substr(1);
      ws_wss_additional_patterns.push_back("ws" +
                                           pattern_without_wildcard_protocol);
      ws_wss_additional_patterns.push_back("wss" +
                                           pattern_without_wildcard_protocol);
    }

    match_patterns.insert(match_patterns.end(),
                          ws_wss_additional_patterns.begin(),
                          ws_wss_additional_patterns.end());
  }

  return match_patterns;
}
}  // namespace

WebUrlPatternNatives::WebUrlPatternNatives(extensions::ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void WebUrlPatternNatives::AddRoutes() {
  RouteHandlerFunction(
      "URLPatternToMatchPatterns",
      base::BindRepeating(&WebUrlPatternNatives::URLPatternToMatchPatterns,
                          base::Unretained(this)));
}

bool V8URLPatternToMatchPatterns(v8::Isolate* isolate,
                                 v8::Local<v8::Value>& input,
                                 std::vector<std::string>& out_match_patterns) {
  std::optional<blink::SafeUrlPattern> safe_url_pattern =
      blink::WebURLPatternToSafeUrlPattern(isolate, input);
  if (!safe_url_pattern) {
    return false;
  }

  if (!WouldBeValidMatchPattern(*safe_url_pattern)) {
    return false;
  }

  out_match_patterns = MatchPatternsFromUrlPattern(*safe_url_pattern);
  return true;
}

// This function sends multiple match patterns back to the js layer, even though
// it gets only one URLPattern object or string via args, because of
// the discrepancies between how URLPatterns and Match Patterns work.
// See the stringifying function above for details.
void WebUrlPatternNatives::URLPatternToMatchPatterns(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsString() || args[0]->IsObject());
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> input = args[0];

  std::vector<std::string> match_patterns;
  if (controlled_frame::V8URLPatternToMatchPatterns(isolate, input,
                                                    match_patterns)) {
    v8::Local<v8::Array> match_patterns_array =
        v8::Array::New(isolate, match_patterns.size());
    for (size_t i = 0; i < match_patterns.size(); ++i) {
      v8::Local<v8::String> str =
          v8::String::NewFromUtf8(isolate, match_patterns[i].c_str(),
                                  v8::NewStringType::kNormal)
              .ToLocalChecked();
      match_patterns_array->Set(context, static_cast<uint32_t>(i), str).Check();
    }

    args.GetReturnValue().Set(match_patterns_array);
  } else {
    args.GetReturnValue().Set(isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, "Invalid URL pattern",
                                v8::NewStringType::kInternalized)
            .ToLocalChecked())));
  }
}

}  // namespace controlled_frame
