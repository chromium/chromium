;// Copyright 2016 The Chromium Authors
;// Use of this source code is governed by a BSD-style license that can be
;// found in the LICENSE file.
;//
;// Defines the names and types of messages that are logged with the SYSLOG
;// macro.
SeverityNames=(Informational=0x0:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x1:STATUS_SEVERITY_WARNING
               Error=0x2:STATUS_SEVERITY_ERROR
               Fatal=0x3:STATUS_SEVERITY_FATAL
              )
FacilityNames=(Browser=0x0:FACILITY_SYSTEM
               ElevationService=0x1:FACILITY_ELEVATION_SERVICE
               EtwService=0x2:FACILITY_TRACING_SERVICE
              )
LanguageNames=(English=0x409:MSG00409)

;// TODO(pastarmovj): Subdivide into more categories if needed.
MessageIdTypedef=WORD

MessageId=0x1
SymbolicName=BROWSER_CATEGORY
Language=English
Browser Events
.

MessageId=0x2
SymbolicName=ELEVATION_SERVICE_CATEGORY
Language=English
Elevation Service Events
.

MessageId=0x3
SymbolicName=TRACING_SERVICE_CATEGORY
Language=English
ETW Service Events
.

MessageIdTypedef=DWORD

MessageId=0x100
Severity=Error
Facility=Browser
SymbolicName=MSG_LOG_MESSAGE
Language=English
%1!S!
.

MessageId=0x101
Facility=ElevationService
SymbolicName=MSG_ELEVATION_SERVICE_LOG_MESSAGE
Language=English
%1!S!
.

MessageId=0x102
Facility=EtwService
SymbolicName=MSG_TRACING_SERVICE_LOG_MESSAGE
Language=English
%1!S!
.
