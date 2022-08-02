// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_TEST_CHROMEDRIVER_LOG_REPLAY_DEVTOOLS_LOG_READER_H_
#define CHROME_TEST_CHROMEDRIVER_LOG_REPLAY_DEVTOOLS_LOG_READER_H_

#include <fstream>
#include <memory>
#include <string>

#include "base/files/file_path.h"

// Represents one DevTools entry (command or response) in the log.
//
// These appear in the log in the following format:
// [<timestamp>][DEBUG]: DevTools <protocol_type> <event_type> <command_name>
//     (id=<id>) <payload>
// where:
//
// <protocol_type> is either HTTP or WebSocket
//
// <event_type> is either "Command:" (for WebSocket only), "Request:" (for HTTP
//   only), "Response:", or "Event:"
//
// <command_name> is the command for WebSocket (like "DOM.getDocument") or a url
// for HTTP (like "http://localhost:38845/json")
//
// <id> is a sequential number to identify WebSocket commands with their
// responses
//
// <socket_id> identifies the WebSocket instance this entry came from, if any.
//
// <payload> is either the parameters in case of a WebSocket command, or the
// response in case of any response. It is always a JSON, and always spans
// multiple lines.
class LogEntry {
 public:
  // Build this LogEntry using the header line of an entry in the log. This
  // doesn't initialize the payload member. The payload must be parsed from
  // lines after the header.
  explicit LogEntry(std::istringstream& header_stream);
  ~LogEntry();

  enum EventType {
    kRequest,  // Command or Request depending on HTTP or WebSocket client
    kResponse,
    kEvent
  };
  enum Protocol { kHTTP, kWebSocket };

  EventType event_type;
  Protocol protocol_type;
  std::string command_name;
  std::string payload;
  int id;
  std::string socket_id;
  bool error;
};

// Reads a log file for DevTools entries.
class DevToolsLogReader {
 public:
  // Initialize the log reader using a path to a log file to read from.
  explicit DevToolsLogReader(const base::FilePath& log_path);
  ~DevToolsLogReader();

  // Get the next DevTools entry in the log of the specified protocol type.
  //
  // This returns commands, responses, and events separately. If there are
  // no remaining entries of the specified type, or if there is some other
  // problem encountered like a truncated JSON, nullptr is returned.
  std::unique_ptr<LogEntry> GetNext(LogEntry::Protocol protocol_type);
  // Undo the previous GetNext call.
  //
  // "Gives back" the unique_ptr to the LogEntry object to the log reader,
  // to be returned the next time GetNext is called.
  void UndoGetNext(std::unique_ptr<LogEntry> next);

 private:
  std::unique_ptr<LogEntry> peeked;
  std::ifstream log_file;
  // Starting with |header_line|, parse a JSON string out of the log file.
  //
  // will parse either list or dictionary-type JSON strings, depending on the
  // starting character.
  std::string GetJSONString(std::istringstream& header_line);

  // Return whether |line| is a header of a DevTools entry.
  //
  // This only parses out the first two words of the line, meaning that the
  // stream can be re-used to parse the specifics of the entry after calling
  // this.
  bool IsHeader(std::istringstream& line) const;

  // Count (number of opening_char) - (number of closing_char) in |line|.
  //
  // Used to check for the end of JSON parameters. Ignores characters inside of
  // non-escaped quotes.
  int CountChar(const std::string& line,
                char opening_char,
                char closing_char) const;
};

#endif  // CHROME_TEST_CHROMEDRIVER_LOG_REPLAY_DEVTOOLS_LOG_READER_H_
