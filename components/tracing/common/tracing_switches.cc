// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/tracing_switches.h"

namespace switches {

// Enables background and upload trace to trace-upload-url. Trigger rules are
// pass as an argument.
const char kEnableBackgroundTracing[] = "enable-background-tracing";

// Causes TRACE_EVENT flags to be recorded from startup.
// This flag will be ignored if --trace-startup or --trace-shutdown is provided.
const char kTraceConfigFile[]               = "trace-config-file";

// Causes TRACE_EVENT flags to be recorded beginning with shutdown. Optionally,
// can specify the specific trace categories to include (e.g.
// --trace-shutdown=base,net) otherwise, all events are recorded.
// --trace-shutdown-file can be used to control where the trace log gets stored
// to since there is otherwise no way to access the result.
const char kTraceShutdown[]                 = "trace-shutdown";

// If supplied, sets the file which shutdown tracing will be stored into, if
// omitted the default will be used "chrometrace.log" in the current directory.
// Has no effect unless --trace-shutdown is also supplied.
// Example: --trace-shutdown --trace-shutdown-file=/tmp/trace_event.log
const char kTraceShutdownFile[]             = "trace-shutdown-file";

// Causes TRACE_EVENT flags to be recorded from startup. Optionally, can
// specify the specific trace categories to include (e.g.
// --trace-startup=base,net) otherwise, all events are recorded. Setting this
// flag results in the first call to BeginTracing() to receive all trace events
// since startup. In Chrome, you may find --trace-startup-file and
// --trace-startup-duration to control the auto-saving of the trace (not
// supported in the base-only TraceLog component).
const char kTraceStartup[]                  = "trace-startup";

// Sets the time in seconds until startup tracing ends. If omitted a default of
// 5 seconds is used. Has no effect without --trace-startup, or if
// --startup-trace-file=none was supplied.
const char kTraceStartupDuration[]          = "trace-startup-duration";

// If supplied, sets the file which startup tracing will be stored into, if
// omitted the default will be used "chrometrace.log" in the current directory.
// Has no effect unless --trace-startup is also supplied.
// Example: --trace-startup --trace-startup-file=/tmp/trace_event.log
// As a special case, can be set to 'none' - this disables automatically saving
// the result to a file and the first manually recorded trace will then receive
// all events since startup.
const char kTraceStartupFile[]              = "trace-startup-file";

// If supplied, sets the tracing record mode; otherwise, the default
// "record-until-full" mode will be used.
const char kTraceStartupRecordMode[] = "trace-startup-record-mode";

// Sends a pretty-printed version of tracing info to the console.
const char kTraceToConsole[]                = "trace-to-console";

// Sets the target URL for uploading tracing data.
const char kTraceUploadURL[]                = "trace-upload-url";

}  // namespace switches
