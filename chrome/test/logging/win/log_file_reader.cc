// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/logging/win/log_file_reader.h"

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging_win.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/win/event_trace_consumer.h"
#include "chrome/test/logging/win/mof_data_parser.h"

namespace logging_win {

namespace {

// TODO(grt) This reverses a mapping produced by base/logging_win.cc's
// LogEventProvider::LogMessage.  LogEventProvider should expose a way to map an
// event level back to a log severity.
logging::LogSeverity EventLevelToSeverity(uint8_t level) {
  switch (level) {
  case TRACE_LEVEL_NONE:
    NOTREACHED();
    return logging::LOG_ERROR;
  case TRACE_LEVEL_FATAL:
    return logging::LOG_FATAL;
  case TRACE_LEVEL_ERROR:
    return logging::LOG_ERROR;
  case TRACE_LEVEL_WARNING:
    return logging::LOG_WARNING;
  case TRACE_LEVEL_INFORMATION:
    return logging::LOG_INFO;
  default:
    // Trace levels above information correspond to negative severity levels,
    // which are used for VLOG verbosity levels.
    return TRACE_LEVEL_INFORMATION - level;
  }
}

class LogFileReader {
 public:
  explicit LogFileReader(LogFileDelegate* delegate);
  ~LogFileReader();

  static void ReadFile(const base::FilePath& log_file,
                       LogFileDelegate* delegate);

 private:
  // An implementation of a trace consumer that delegates to a given (at
  // compile-time) event processing function.
  template<void (*ProcessEventFn)(EVENT_TRACE*)>
  class TraceConsumer
      : public base::win::EtwTraceConsumerBase<TraceConsumer<ProcessEventFn> > {
   public:
    TraceConsumer() { }
    static void ProcessEvent(EVENT_TRACE* event) { (*ProcessEventFn)(event); }
   private:
    DISALLOW_COPY_AND_ASSIGN(TraceConsumer);
  };

  // Delegates to DispatchEvent() of the current LogDumper instance.
  static void ProcessEvent(EVENT_TRACE* event);

  // Handlers for the supported event types.
  bool OnLogMessageEvent(const EVENT_TRACE* event);
  bool OnLogMessageFullEvent(const EVENT_TRACE* event);
  bool OnFileHeader(const EVENT_TRACE* event);

  // Parses an event and passes it along to the delegate for processing.
  void DispatchEvent(const EVENT_TRACE* event);

  // Reads the file using a trace consumer.  |ProcessEvent| will be invoked for
  // each event in the file.
  void Read(const base::FilePath& log_file);

  // Protects use of the class; only one instance may be live at a time.
  static base::LazyInstance<base::Lock>::Leaky reader_lock_;

  // The currently living instance.
  static LogFileReader* instance_;

  // The delegate to be notified of events.
  LogFileDelegate* delegate_;
};

// static
base::LazyInstance<base::Lock>::Leaky LogFileReader::reader_lock_ =
    LAZY_INSTANCE_INITIALIZER;

// static
LogFileReader* LogFileReader::instance_ = NULL;

LogFileReader::LogFileReader(LogFileDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(instance_ == NULL);
  DCHECK(delegate != NULL);
  instance_ = this;
}

LogFileReader::~LogFileReader() {
  DCHECK_EQ(instance_, this);
  instance_ = NULL;
}

// static
void LogFileReader::ProcessEvent(EVENT_TRACE* event) {
  if (instance_ != NULL)
    instance_->DispatchEvent(event);
}

bool LogFileReader::OnLogMessageEvent(const EVENT_TRACE* event) {
  base::StringPiece message;
  MofDataParser parser(event);

  // See LogEventProvider::LogMessage where ENABLE_LOG_MESSAGE_ONLY is set.
  if (parser.ReadString(&message) && parser.empty()) {
    delegate_->OnLogMessage(event,
                            EventLevelToSeverity(event->Header.Class.Level),
                            message);
    return true;
  }
  return false;
}

bool LogFileReader::OnLogMessageFullEvent(const EVENT_TRACE* event) {
  DWORD stack_depth = 0;
  const intptr_t* backtrace = NULL;
  int line = 0;
  base::StringPiece file;
  base::StringPiece message;
  MofDataParser parser(event);

  // See LogEventProvider::LogMessage where ENABLE_LOG_MESSAGE_ONLY is not set.
  if (parser.ReadDWORD(&stack_depth) &&
      parser.ReadPointerArray(stack_depth, &backtrace) &&
      parser.ReadInt(&line) &&
      parser.ReadString(&file) &&
      parser.ReadString(&message) &&
      parser.empty()) {
    delegate_->OnLogMessageFull(event,
        EventLevelToSeverity(event->Header.Class.Level), stack_depth, backtrace,
        line, file, message);
    return true;
  }
  return false;
}

bool LogFileReader::OnFileHeader(const EVENT_TRACE* event) {
  MofDataParser parser(event);
  const TRACE_LOGFILE_HEADER* header = NULL;

  if (parser.ReadStructure(&header)) {
    delegate_->OnFileHeader(event, header);
    return true;
  }
  return false;
}

void LogFileReader::DispatchEvent(const EVENT_TRACE* event) {
  bool parsed = true;

  if (IsEqualGUID(event->Header.Guid, logging::kLogEventId)) {
    if (event->Header.Class.Type == logging::LOG_MESSAGE)
      parsed = OnLogMessageEvent(event);
    else if (event->Header.Class.Type == logging::LOG_MESSAGE_FULL)
      parsed = OnLogMessageFullEvent(event);
  } else if (IsEqualGUID(event->Header.Guid, EventTraceGuid)) {
    parsed = OnFileHeader(event);
  } else {
    DCHECK(parsed);
    delegate_->OnUnknownEvent(event);
  }
  if (!parsed)
    delegate_->OnUnparsableEvent(event);
}

void LogFileReader::Read(const base::FilePath& log_file) {
  TraceConsumer<&ProcessEvent> consumer;
  HRESULT hr = S_OK;

  hr = consumer.OpenFileSession(log_file.value().c_str());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to open session for log file " << log_file.value()
               << "; hr=" << std::hex << hr;
  } else {
    consumer.Consume();
    consumer.Close();
  }
}

// static
void LogFileReader::ReadFile(const base::FilePath& log_file,
                             LogFileDelegate* delegate) {
  base::AutoLock lock(reader_lock_.Get());

  LogFileReader(delegate).Read(log_file);
}

}  // namespace

LogFileDelegate::LogFileDelegate() {
}

LogFileDelegate::~LogFileDelegate() {
}

void ReadLogFile(const base::FilePath& log_file, LogFileDelegate* delegate) {
  DCHECK(delegate);
  LogFileReader::ReadFile(log_file, delegate);
}

}  // namespace logging_win
