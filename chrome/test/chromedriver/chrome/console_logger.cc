// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/console_logger.h"

#include <stddef.h>

#include <string>

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
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
  base::Value::Dict params;
  Status status = client->SendCommand("Log.enable", params);
  if (status.IsError()) {
    return status;
  }
  return client->SendCommand("Runtime.enable", params);
}

Status ConsoleLogger::OnEvent(DevToolsClient* client,
                              const std::string& method,
                              const base::Value::Dict& params) {
  if (method == "Log.entryAdded")
    return OnLogEntryAdded(params);
  if (method == "Runtime.consoleAPICalled")
    return OnRuntimeConsoleApiCalled(params);
  if (method == "Runtime.exceptionThrown")
    return OnRuntimeExceptionThrown(params);
  return Status(kOk);
}

Status ConsoleLogger::OnLogEntryAdded(const base::Value::Dict& params) {
  const base::Value::Dict* entry = params.FindDict("entry");
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
    const base::Value::Dict& params) {
  const std::string* type = params.FindString("type");
  if (!type)
    return Status(kUnknownError, "missing or invalid type");
  Log::Level level;
  if (!ConsoleLevelToLogLevel(*type, &level))
    return Status(kOk);

  std::string origin = "console-api";
  std::string line_column = "-";
  const base::Value::Dict* stack_trace = params.FindDict("stackTrace");
  if (stack_trace) {
    const base::Value::List* call_frames = stack_trace->FindList("callFrames");
    if (!call_frames)
      return Status(kUnknownError, "missing or invalid callFrames");
    const base::Value& call_frame_value = call_frames->front();
    const base::Value::Dict* call_frame = call_frame_value.GetIfDict();
    if (call_frame) {
      const std::string* url = call_frame->FindString("url");
      if (!url)
        return Status(kUnknownError, "missing or invalid url");
      if (!url->empty())
        origin = *url;
      int line = call_frame->FindInt("lineNumber").value_or(-1);
      if (line < 0)
        return Status(kUnknownError, "missing or invalid lineNumber");
      int column = call_frame->FindInt("columnNumber").value_or(-1);
      if (column < 0)
        return Status(kUnknownError, "missing or invalid columnNumber");
      line_column = base::StringPrintf("%d:%d", line, column);
    }
  }

  const base::Value::List* args = params.FindList("args");
  if (!args || args->empty())
    return Status(kUnknownError, "missing or invalid args");

  std::string text;
  int arg_count = args->size();
  for (int i = 0; i < arg_count; i++) {
    const base::Value::Dict* current_arg = (*args)[i].GetIfDict();
    if (!current_arg) {
      std::string error_message = base::StringPrintf("Argument %d is missing or invalid", i);
      return Status(kUnknownError, error_message);
    }
    std::string temp_text;
    const std::string* arg_type = current_arg->FindString("type");
    if (arg_type && *arg_type == "undefined") {
      temp_text = "undefined";
    } else {
      const std::string* description = current_arg->FindString("description");
      if (description) {
        temp_text = *description;
      } else {
        const base::Value* value = current_arg->Find("value");
        if (!value) {
          return Status(kUnknownError, "missing or invalid arg value");
        }
        if (!base::JSONWriter::Write(*value, &temp_text)) {
          return Status(kUnknownError, "failed to convert value to text");
        }
      }
    }
    // Add spaces between the arguments.
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
    const base::Value::Dict& params) {
  const base::Value::Dict* exception_details =
      params.FindDict("exceptionDetails");
  ;
  if (!exception_details)
    return Status(kUnknownError, "missing or invalid exception details");

  std::string origin;
  if (const std::string* temp_origin = exception_details->FindString("url"))
    origin = *temp_origin;
  else
    origin = "javascript";

  int line = exception_details->FindInt("lineNumber").value_or(-1);
  if (line < 0)
    return Status(kUnknownError, "missing or invalid lineNumber");
  int column = exception_details->FindInt("columnNumber").value_or(-1);
  if (column < 0)
    return Status(kUnknownError, "missing or invalid columnNumber");
  std::string line_column = base::StringPrintf("%d:%d", line, column);

  std::string text;
  const base::Value::Dict* exception = exception_details->FindDict("exception");
  const base::Value::Dict* preview =
      exception ? exception->FindDict("preview") : nullptr;
  const base::Value::List* properties =
      preview ? preview->FindList("properties") : nullptr;
  if (properties) {
    // If the event contains an object which is an instance of the JS Error
    // class, attempt to get the message property for the exception.
    for (const base::Value& property_value : *properties) {
      const base::Value::Dict* dict = property_value.GetIfDict();
      if (dict) {
        const std::string* name = dict->FindString("name");
        if (name && *name == "message") {
          const std::string* value = dict->FindString("value");
          if (value) {
            text = *value;
            const std::string* class_name = exception->FindString("className");
            if (class_name)
              text = "Uncaught " + *class_name + ": " + text;
            break;
          }
        }
      }
    }
  } else {
    // Since |exception.preview.properties| is optional, fall back to |text|
    // (which is required) if we don't find anything.
    if (const std::string* temp_text = exception_details->FindString("text"))
      text = *temp_text;
    else
      return Status(kUnknownError, "missing or invalid exception message text");
  }

  log_->AddEntry(Log::kError, "javascript", base::StringPrintf(
      "%s %s %s", origin.c_str(), line_column.c_str(), text.c_str()));
  return Status(kOk);
}
