// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/log_replay/devtools_log_reader.h"

#include <iostream>
#include <string>

#include "base/logging.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"

namespace {
// Parses the word (id=X) and just returns the id number
int GetId(std::istringstream& header_stream) {
  int id = 0;
  header_stream.ignore(5);  // ignore the " (id=" characters
  header_stream >> id;
  header_stream.ignore(1);  // ignore the final parenthesis
  return id;
}

void putback(std::istringstream& stream, const std::string& str) {
  for (auto it = str.rbegin(); it != str.rend(); ++it) {
    stream.putback(*it);
  }
}

// Parses the word (session_id=X) and just returns the session_id string
bool GetSessionId(std::istringstream& header_stream, std::string* session_id) {
  std::string spaces;
  while (base::IsAsciiWhitespace(header_stream.peek())) {
    char ch;
    header_stream.get(ch);
    spaces.push_back(ch);
  }
  std::string sid;  // (session_id=X)
  header_stream >> sid;
  if (sid.size() < 13  // (session_id=) is also valid
      || sid.find("(session_id=") != 0 || sid.back() != ')') {
    putback(header_stream, sid);
    putback(header_stream, spaces);
    return false;
  }

  // skip 12 leading characters: (session_id=
  // and one trailing character: )
  *session_id = sid.substr(12, sid.size() - 12 - 1);
  return true;
}

}  // namespace

LogEntry::LogEntry(std::istringstream& header_stream) {
  error = false;
  std::string protocol_type_string;
  header_stream >> protocol_type_string;  // "HTTP" or "WebSocket"
  if (protocol_type_string == "HTTP") {
    protocol_type = kHTTP;
  } else if (protocol_type_string == "WebSocket") {
    protocol_type = kWebSocket;
  } else {
    error = true;
    LOG(ERROR) << "Could not read protocol from log entry header.";
    return;
  }

  std::string event_type_string;
  header_stream >> event_type_string;

  if (event_type_string == "Response:") {
    event_type = kResponse;
  } else if (event_type_string == "Command:" ||
             event_type_string == "Request:") {
    event_type = kRequest;
  } else if (event_type_string == "Event:") {
    event_type = kEvent;
  } else {
    error = true;
    LOG(ERROR) << "Could not read event type from log entry header.";
    return;
  }

  if (!(protocol_type == kHTTP && event_type == kResponse)) {
    header_stream >> command_name;
    if (command_name == "") {
      error = true;
      LOG(ERROR) << "Could not read command name from log entry header";
      return;
    }
    if (protocol_type != kHTTP) {
      if (event_type != kEvent) {
        id = GetId(header_stream);
        if (id == 0) {
          error = true;
          LOG(ERROR) << "Could not read sequential id from log entry header.";
          return;
        }
      }

      session_id.clear();
      if (!GetSessionId(header_stream, &session_id)) {
        error = true;
        LOG(ERROR) << "Could not read session_id from log entry header.";
      }

      header_stream >> socket_id;
      if (socket_id == "") {
        error = true;
        LOG(ERROR) << "Could not read socket id from log entry header.";
      }
    }
  }
}

LogEntry::~LogEntry() {}

DevToolsLogReader::DevToolsLogReader(const base::FilePath& log_path)
    : log_file(log_path.value().c_str(), std::ios::in) {}

DevToolsLogReader::~DevToolsLogReader() {}

bool DevToolsLogReader::IsHeader(std::istringstream& header_stream) const {
  std::string word;
  header_stream >> word;  // preamble
  if (!base::MatchPattern(word, "[??????????.???][DEBUG]:") &&
      !base::MatchPattern(word, "[??????????")) {
    return false;
  }
  header_stream >> word;  // "DevTools" for DevTools commands/responses/events
  // test for the second half of readable timestamp and read next token
  if (base::MatchPattern(word, "????????.??????][DEBUG]:")) {
    header_stream >> word;
  }
  bool result = word == "DevTools";
  return result;
}

void DevToolsLogReader::UndoGetNext(std::unique_ptr<LogEntry> next) {
  peeked = std::move(next);
}

std::unique_ptr<LogEntry> DevToolsLogReader::GetNext(
    LogEntry::Protocol protocol_type) {
  if (peeked) {
    return std::move(peeked);
  }
  std::string next_line;
  while (true) {
    if (log_file.eof())
      return nullptr;
    std::getline(log_file, next_line);

    std::istringstream next_line_stream(next_line);
    if (IsHeader(next_line_stream)) {
      std::unique_ptr<LogEntry> log_entry =
          std::make_unique<LogEntry>(next_line_stream);
      if (log_entry->error) {
        return nullptr;  // helpful error message already logged
      }
      if (log_entry->protocol_type != protocol_type)
        continue;
      if (!(log_entry->event_type == LogEntry::kRequest &&
            log_entry->protocol_type == LogEntry::kHTTP)) {
        log_entry->payload = GetJSONString(next_line_stream);
        if (log_entry->payload == "") {
          LOG(ERROR) << "Problem parsing JSON from log file";
          return nullptr;
        }
      }
      return log_entry;
    }
  }
}

std::string DevToolsLogReader::GetJSONString(
    std::istringstream& header_stream) {
  std::string next_line, json;

  int opening_char_count = 0;
  std::getline(header_stream, next_line);
  next_line = next_line.substr(1);
  char opening_char = next_line[0];
  char closing_char;
  switch (opening_char) {
    case '{':
      closing_char = '}';
      break;
    case '[':
      closing_char = ']';
      break;
    default:
      return next_line;  // For rare cases when payload is a string, not a JSON.
  }
  while (true) {
    json += next_line + "\n";
    opening_char_count += CountChar(next_line, opening_char, closing_char);
    if (opening_char_count == 0)
      break;
    if (log_file.eof())
      return "";
    getline(log_file, next_line);
  }
  return json;
}

int DevToolsLogReader::CountChar(const std::string& line,
                                 char opening_char,
                                 char closing_char) const {
  bool in_quote = false;
  int total = 0;
  for (size_t i = 0; i < line.length(); i++) {
    if (!in_quote && line[i] == opening_char)
      total++;
    if (!in_quote && line[i] == closing_char)
      total--;
    if (line[i] == '"' && (i == 0 || line[i - 1] != '\\'))
      in_quote = !in_quote;
  }
  return total;
}
