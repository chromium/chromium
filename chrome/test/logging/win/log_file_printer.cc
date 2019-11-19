// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/logging/win/log_file_printer.h"

#include <windows.h>
#include <objbase.h>
#include <stddef.h>

#include <iomanip>
#include <ios>
#include <ostream>
#include <sstream>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/win_util.h"
#include "chrome/test/logging/win/log_file_reader.h"

namespace {

// TODO(grt) This duplicates private behavior in base/logging.cc's
// LogMessage::Init.  That behavior should be exposed and used here (possibly
// by moving this function to logging.cc, making it use log_severity_names, and
// publishing it in logging.h with BASE_EXPORT).
void WriteSeverityToStream(logging::LogSeverity severity, std::ostream* out) {
  switch (severity) {
    case logging::LOG_INFO:
      *out << "INFO";
      break;
    case logging::LOG_WARNING:
      *out << "WARNING";
      break;
    case logging::LOG_ERROR:
      *out << "ERROR";
      break;
    case logging::LOG_FATAL:
      *out << "FATAL";
      break;
    default:
      if (severity < 0)
        *out << "VERBOSE" << -severity;
      else
        NOTREACHED();
      break;
  }
}

// TODO(grt) This duplicates private behavior in base/logging.cc's
// LogMessage::Init.  That behavior should be exposed and used here (possibly
// by moving this function to logging.cc and publishing it in logging.h with
// BASE_EXPORT).
void WriteLocationToStream(const base::StringPiece& file,
                           int line,
                           std::ostream* out) {
  base::StringPiece filename(file);
  size_t last_slash_pos = filename.find_last_of("\\/");
  if (last_slash_pos != base::StringPiece::npos)
    filename.remove_prefix(last_slash_pos + 1);

  *out << filename << '(' << line << ')';
}

class EventPrinter : public logging_win::LogFileDelegate {
 public:
  explicit EventPrinter(std::ostream* out);
  ~EventPrinter() override;

  void OnUnknownEvent(const EVENT_TRACE* event) override;

  void OnUnparsableEvent(const EVENT_TRACE* event) override;

  void OnFileHeader(const EVENT_TRACE* event,
                    const TRACE_LOGFILE_HEADER* header) override;

  void OnLogMessage(const EVENT_TRACE* event,
                    logging::LogSeverity severity,
                    const base::StringPiece& message) override;

  void OnLogMessageFull(const EVENT_TRACE* event,
                        logging::LogSeverity severity,
                        DWORD stack_depth,
                        const intptr_t* backtrace,
                        int line,
                        const base::StringPiece& file,
                        const base::StringPiece& message) override;

 private:
  void PrintTimeStamp(LARGE_INTEGER time_stamp);
  void PrintEventContext(const EVENT_TRACE* event,
                         const base::StringPiece& level,
                         const base::StringPiece& context);
  void PrintBadEvent(const EVENT_TRACE* event, const base::StringPiece& error);

  std::ostream* out_;
  DISALLOW_COPY_AND_ASSIGN(EventPrinter);
};

EventPrinter::EventPrinter(std::ostream* out)
    : out_(out) {
}

EventPrinter::~EventPrinter() {
}

void EventPrinter::PrintTimeStamp(LARGE_INTEGER time_stamp) {
  FILETIME event_time = {};
  base::Time::Exploded time_exploded = {};
  event_time.dwLowDateTime = time_stamp.LowPart;
  event_time.dwHighDateTime = time_stamp.HighPart;
  base::Time::FromFileTime(event_time).LocalExplode(&time_exploded);

  *out_ << std::setfill('0')
        << std::setw(2) << time_exploded.month
        << std::setw(2) << time_exploded.day_of_month
        << '/'
        << std::setw(2) << time_exploded.hour
        << std::setw(2) << time_exploded.minute
        << std::setw(2) << time_exploded.second
        << '.'
        << std::setw(3) << time_exploded.millisecond;
}

// Prints the context info at the start of each line: pid, tid, time, etc.
void EventPrinter::PrintEventContext(const EVENT_TRACE* event,
                                     const base::StringPiece& level,
                                     const base::StringPiece& context) {
  *out_ << '[' << event->Header.ProcessId << ':'
        << event->Header.ThreadId << ':';
  PrintTimeStamp(event->Header.TimeStamp);
  if (!level.empty())
    *out_ << ':' << level;
  if (!context.empty())
    *out_ << ':' << context;
  *out_ << "] ";
}

// Prints a useful message for events that can't be otherwise printed.
void EventPrinter::PrintBadEvent(const EVENT_TRACE* event,
                                 const base::StringPiece& error) {
  *out_ << error
        << " (class=" << base::win::String16FromGUID(event->Header.Guid)
        << ", type=" << static_cast<int>(event->Header.Class.Type) << ")";
}

void EventPrinter::OnUnknownEvent(const EVENT_TRACE* event) {
  base::StringPiece empty;
  PrintEventContext(event, empty, empty);
  PrintBadEvent(event, "unsupported event");
}

void EventPrinter::OnUnparsableEvent(const EVENT_TRACE* event) {
  base::StringPiece empty;
  PrintEventContext(event, empty, empty);
  PrintBadEvent(event, "parse error");
}

void EventPrinter::OnFileHeader(const EVENT_TRACE* event,
                                const TRACE_LOGFILE_HEADER* header) {
  base::StringPiece empty;
  PrintEventContext(event, empty, empty);

  *out_ << "Log captured from Windows "
        << static_cast<int>(header->VersionDetail.MajorVersion) << '.'
        << static_cast<int>(header->VersionDetail.MinorVersion) << '.'
        << static_cast<int>(header->VersionDetail.SubVersion) << '.'
        << static_cast<int>(header->VersionDetail.SubMinorVersion)
        << ".  " << header->EventsLost << " events lost, "
        << header->BuffersLost << " buffers lost." << std::endl;
}

void EventPrinter::OnLogMessage(const EVENT_TRACE* event,
                                logging::LogSeverity severity,
                                const base::StringPiece& message) {
  std::ostringstream level_stream;
  WriteSeverityToStream(severity, &level_stream);
  PrintEventContext(event, level_stream.str(), base::StringPiece());
  *out_ << message << std::endl;
}

void EventPrinter::OnLogMessageFull(const EVENT_TRACE* event,
                                    logging::LogSeverity severity,
                                    DWORD stack_depth,
                                    const intptr_t* backtrace,
                                    int line,
                                    const base::StringPiece& file,
                                    const base::StringPiece& message) {
  std::ostringstream level_stream;
  std::ostringstream location_stream;
  WriteSeverityToStream(severity, &level_stream);
  WriteLocationToStream(file, line, &location_stream);
  PrintEventContext(event, level_stream.str(), location_stream.str());
  *out_ << message << std::endl;
}

}  // namespace

void logging_win::PrintLogFile(const base::FilePath& log_file,
                               std::ostream* out) {
  EventPrinter printer(out);
  logging_win::ReadLogFile(log_file, &printer);
}
