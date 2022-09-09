// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/console_logger.h"

#include <stddef.h>

#include <string>

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"

namespace {

// Translates DevTools log level strings into Log::Level.
bool ConsoleLevelToLogLevel(const std::string& name, Log::Level *out_level) {
  if (name == "verbose" || name == "debug" || name == "timeEnd")
    *out_level = Log::kDebug;
  else if (name == "log" || name == "info")
    *out_level = Log::kInfo;
  else if (name == "warning")
    *out_level = Log::kWarning;
  else if (name == "error" || name == "assert")
    *out_level = Log::kError;
  else
    return false;
  return true;
}

}  // namespace

ConsoleLogger::ConsoleLogger(Log* log)
    : log_(log) {}

Status ConsoleLogger::OnConnected(DevToolsClient* client) {
  base::DictionaryValue params;
  Status status = client->SendCommand("Log.enable", params);
  if (status.IsError()) {
    return status;
  }
  return client->SendCommand("Runtime.enable", params);
}

Status ConsoleLogger::OnEvent(
    DevToolsClient* client,
    const std::string& method,
    const base::DictionaryValue& params) {
  if (method == "Log.entryAdded")
    return OnLogEntryAdded(params);
  if (method == "Runtime.consoleAPICalled")
    return OnRuntimeConsoleApiCalled(params);
  if (method == "Runtime.exceptionThrown")
    return OnRuntimeExceptionThrown(params);
  return Status(kOk);
}

Status ConsoleLogger::OnLogEntryAdded(const base::DictionaryValue& params) {
  const base::Value::Dict* entry = params.GetDict().FindDict("entry");
  if (!entry)
    return Status(kUnknownError, "missing or invalid 'entry'");

  const std::string* level_name = entry->FindString("level");
  Log::Level level;
  if (!level_name || !ConsoleLevelToLogLevel(*level_name, &level))
    return Status(kUnknownError, "missing or invalid 'entry.level'");

  const std::string* source = entry->FindString("source");
  if (!source)
    return Status(kUnknownError, "missing or invalid 'entry.source'");

  const std::string* origin = entry->FindString("url");
  if (!origin)
    origin = source;

  std::string line_number;
  int line = entry->FindInt("lineNumber").value_or(-1);
  if (line >= 0) {
    line_number = base::StringPrintf("%d", line);
  } else {
    // No line number, but print anyway, just to maintain the number of fields
    // in the formatted message in case someone wants to parse it.
    line_number = "-";
  }

  const std::string* text = entry->FindString("text");
  if (!text)
    return Status(kUnknownError, "missing or invalid 'entry.text'");

  log_->AddEntry(level, *source,
                 base::StringPrintf("%s %s %s", origin->c_str(),
                                    line_number.c_str(), text->c_str()));
  return Status(kOk);
}

Status ConsoleLogger::OnRuntimeConsoleApiCalled(
    const base::DictionaryValue& params) {
  std::string type;
  if (!params.GetString("type", &type))
    return Status(kUnknownError, "missing or invalid type");
  Log::Level level;
  if (!ConsoleLevelToLogLevel(type, &level))
    return Status(kOk);

  std::string origin = "console-api";
  std::string line_column = "-";
  const base::DictionaryValue* stack_trace = nullptr;
  if (params.GetDictionary("stackTrace", &stack_trace)) {
    const base::ListValue* call_frames = nullptr;
    if (!stack_trace->GetList("callFrames", &call_frames))
      return Status(kUnknownError, "missing or invalid callFrames");
    const base::Value& call_frame_value = call_frames->GetList()[0];
    if (call_frame_value.is_dict()) {
      const base::DictionaryValue& call_frame =
          base::Value::AsDictionaryValue(call_frame_value);
      std::string url;
      if (!call_frame.GetString("url", &url))
        return Status(kUnknownError, "missing or invalid url");
      if (!url.empty())
        origin = url;
      int line = call_frame.FindIntKey("lineNumber").value_or(-1);
      if (line < 0)
        return Status(kUnknownError, "missing or invalid lineNumber");
      int column = call_frame.FindIntKey("columnNumber").value_or(-1);
      if (column < 0)
        return Status(kUnknownError, "missing or invalid columnNumber");
      line_column = base::StringPrintf("%d:%d", line, column);
    }
  }

  std::string text;
  const base::ListValue* args = nullptr;

  if (!params.GetList("args", &args) || args->GetList().size() < 1) {
    return Status(kUnknownError, "missing or invalid args");
  }

  int arg_count = args->GetList().size();
  for (int i = 0; i < arg_count; i++) {
    const base::Value& current_arg_value = args->GetList()[i];
    if (!current_arg_value.is_dict()) {
      std::string error_message = base::StringPrintf("Argument %d is missing or invalid", i);
      return Status(kUnknownError, error_message );
    }
    const base::DictionaryValue& current_arg =
        base::Value::AsDictionaryValue(current_arg_value);
    std::string temp_text;
    std::string arg_type;
    if (current_arg.GetString("type", &arg_type) && arg_type == "undefined") {
      temp_text = "undefined";
    } else if (!current_arg.GetString("description", &temp_text)) {
      const base::Value* value = current_arg.FindKey("value");
      if (value == nullptr) {
        return Status(kUnknownError, "missing or invalid arg value");
      }
      if (!base::JSONWriter::Write(*value, &temp_text)) {
        return Status(kUnknownError, "failed to convert value to text");
      }
    }
    // add spaces between the arguments
    if (i != 0)
      text += " ";
    text += temp_text;
   }

  log_->AddEntry(level, "console-api", base::StringPrintf("%s %s %s",
                                                          origin.c_str(),
                                                          line_column.c_str(),
                                                          text.c_str()));
  return Status(kOk);
}

Status ConsoleLogger::OnRuntimeExceptionThrown(
    const base::DictionaryValue& params) {
  const base::DictionaryValue* exception_details = nullptr;

  if (!params.GetDictionary("exceptionDetails", &exception_details))
      return Status(kUnknownError, "missing or invalid exception details");

  std::string origin;
  if (!exception_details->GetString("url", &origin))
    origin = "javascript";

  int line = exception_details->FindIntKey("lineNumber").value_or(-1);
  if (line < 0)
    return Status(kUnknownError, "missing or invalid lineNumber");
  int column = exception_details->FindIntKey("columnNumber").value_or(-1);
  if (column < 0)
    return Status(kUnknownError, "missing or invalid columnNumber");
  std::string line_column = base::StringPrintf("%d:%d", line, column);

  std::string text;
  const base::DictionaryValue* exception = nullptr;
  const base::DictionaryValue* preview = nullptr;
  const base::ListValue* properties = nullptr;
  if (exception_details->GetDictionary("exception", &exception) &&
      exception->GetDictionary("preview", &preview) &&
      preview->GetList("properties", &properties)) {
    // If the event contains an object which is an instance of the JS Error
    // class, attempt to get the message property for the exception.
    for (const base::Value& property_value : properties->GetList()) {
      if (property_value.is_dict()) {
        const base::DictionaryValue& property =
            base::Value::AsDictionaryValue(property_value);
        std::string name;
        if (property.GetString("name", &name) && name == "message") {
          if (property.GetString("value", &text)) {
            std::string class_name;
            if (exception->GetString("className", &class_name))
              text = "Uncaught " + class_name + ": " + text;
            break;
          }
        }
      }
    }
  } else {
    // Since |exception.preview.properties| is optional, fall back to |text|
    // (which is required) if we don't find anything.
    if (!exception_details->GetString("text", &text))
      return Status(kUnknownError, "missing or invalid exception message text");
  }

  log_->AddEntry(Log::kError, "javascript", base::StringPrintf(
      "%s %s %s", origin.c_str(), line_column.c_str(), text.c_str()));
  return Status(kOk);
}
