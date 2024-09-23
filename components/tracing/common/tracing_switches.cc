// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/tracing_switches.h"

namespace switches {

// Enables background tracing by passing a scenarios config as an argument. The
// config is a serialized proto `perfetto.protos.ChromeFieldTracingConfig`
// defined in
// third_party/perfetto/protos/perfetto/config/chrome/scenario_config.proto.
// protoc can be used to generate a serialized proto config with
// protoc
//   --encode=perfetto.protos.ChromeFieldTracingConfig
//   --proto_path=third_party/perfetto/
//     third_party/perfetto/protos/perfetto/config/chrome/scenario_config.proto
//  < {input txt config}.pbtxt > {output proto config}.pb
const char kEnableBackgroundTracing[] = "enable-background-tracing";

// Enables background tracing by passing legacy trigger rules as an argument.
const char kEnableLegacyBackgroundTracing[] =
    "enable-legacy-background-tracing";

// Causes TRACE_EVENT flags to be recorded from startup.
// This flag will be ignored if --trace-startup or --trace-shutdown is provided.
const char kTraceConfigFile[]               = "trace-config-file";

// Causes TRACE_EVENT flags to be recorded from startup. Optionally, can
// specify the specific trace categories to include (e.g.
// --trace-startup=base,net) otherwise, all events are recorded. Setting this
// flag results in the first call to BeginTracing() to receive all trace events
// since startup.
//
// Historically, --trace-startup was used for browser startup profiling and
// --enable-tracing was used for browsertest tracing. Now they are share the
// same implementation, but both are still supported to avoid disrupting
// existing workflows. The only difference between them is the default duration
// (5 seconds for trace-startup, unlimited for enable-tracing). If both are
// specified, 'trace-startup' takes precedence.
//
// In Chrome, you may find --trace-startup-file and
// --trace-startup-duration to control the auto-saving of the trace (not
// supported in the base-only TraceLog component).
const char kTraceStartup[] = "trace-startup";
const char kEnableTracing[] = "enable-tracing";

// Causes TRACE_EVENT flags to be recorded from startup, passing a SMB
// handle containing the serialized perfetto config. This flag will be
// ignored if --trace-startup or --trace-shutdown is provided.
const char kTraceConfigHandle[] = "trace-config-handle";

// Sets the time in seconds until startup tracing ends. If omitted:
// - if --trace-startup is specified, a default of 5 seconds is used.
// - if --enable-tracing is specified, tracing lasts until the browser is
// closed. Has no effect otherwise.
const char kTraceStartupDuration[]          = "trace-startup-duration";

// If supplied, sets the file which startup tracing will be stored into, if
// omitted the default will be used "chrometrace.log" in the current directory.
// Has no effect unless --trace-startup is also supplied.
// Example: --trace-startup --trace-startup-file=/tmp/trace_event.log
// As a special case, can be set to 'none' - this disables automatically saving
// the result to a file and the first manually recorded trace will then receive
// all events since startup.
const char kTraceStartupFile[] = "trace-startup-file";

// Similar to the flag above, with the following differences:
// - A more detailed basename will be generated.
// - If the value is empty or ends with path separator, the provided directory
// will be used (with empty standing for current directory) and a detailed
// basename file will be generated.
//
// It is ignored if --trace-startup-file is specified.
const char kEnableTracingOutput[] = "enable-tracing-output";

// Sets the output format for the trace, valid values are "json" and "proto".
// If not set, the current default is "proto".
// "proto", unlike json, supports writing the trace into the output file
// incrementally and is more likely to retain more data if the browser process
// unexpectedly terminates.
// Ignored if "trace-startup-owner" is not "controller".
const char kTraceStartupFormat[] = "trace-startup-format";
const char kEnableTracingFormat[] = "enable-tracing-format";

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

// Repeat internable data for each TraceEvent in the perfetto proto format.
const char kPerfettoDisableInterning[] = "perfetto-disable-interning";

// Sends a pretty-printed version of tracing info to the console.
const char kTraceToConsole[] = "trace-to-console";

// Sets a local folder destination for tracing data. This is only used if
// kEnableBackgroundTracing is also specified.
const char kBackgroundTracingOutputPath[] = "background-tracing-output-path";

// Configures the size of the shared memory buffer used for tracing. Value is
// provided in kB. Defaults to 4096. Should be a multiple of the SMB page size
// (currently 32kB on Desktop or 4kB on Android).
const char kTraceSmbSize[] = "trace-smb-size";

// This is only used when we did not set buffer size in trace config and will be
// used for all trace sessions. If not provided, we will use the default value
// provided in perfetto_config.cc
const char kDefaultTraceBufferSizeLimitInKb[] =
    "default-trace-buffer-size-limit-in-kb";

}  // namespace switches
