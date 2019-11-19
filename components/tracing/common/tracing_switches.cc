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

// If supplied, sets the tracing record mode and options; otherwise, the default
// "record-until-full" mode will be used.
const char kTraceStartupRecordMode[] = "trace-startup-record-mode";

// Specifies the coordinator of the startup tracing session. If the legacy
// tracing backend is used instead of perfetto, providing this flag is not
// necessary. Valid values: 'controller', 'devtools', or 'system'. Defaults to
// 'controller'.
//
// If 'controller' is specified, the session is controlled and stopped via the
// TracingController (e.g. to implement the timeout).
//
// If 'devtools' is specified, the startup tracing session will be owned by
// DevTools and thus can be controlled (i.e. stopped) via the DevTools Tracing
// domain on the first session connected to the browser endpoint.
//
// If 'system' is specified, the system Perfetto service should already be
// tracing on a supported platform (currently only Android). Session is stopped
// through the normal methods for stopping system traces.
const char kTraceStartupOwner[] = "trace-startup-owner";

// If the perfetto tracing backend is used, this enables privacy filtering in
// the TraceEvent data sources for the startup tracing session.
const char kTraceStartupEnablePrivacyFiltering[] =
    "trace-startup-enable-privacy-filtering";

// Repeat internable data for each TraceEvent in the perfetto proto format.
const char kPerfettoDisableInterning[] = "perfetto-disable-interning";

// If supplied, will enable Perfetto startup tracing and stream the
// output to the given file. On Android, if no file is provided, automatically
// generate a file to write the output to.
// TODO(oysteine): Remove once Perfetto starts early enough after
// process startup to be able to replace the legacy startup tracing.
const char kPerfettoOutputFile[] = "perfetto-output-file";

// Sends a pretty-printed version of tracing info to the console.
const char kTraceToConsole[]                = "trace-to-console";

// Sets the target URL for uploading tracing data.
const char kTraceUploadURL[]                = "trace-upload-url";

}  // namespace switches
