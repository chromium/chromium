@echo off
REM Copyright 2023 The Chromium Authors
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.

REM Example of setup script that writes the app 'pv' value into the registry.

set REG_HIVE=HKCU
set COMPANY=Google
set APPID={AE098195-B8DB-4A49-8E23-84FCACB61FF1}
set PRODUCT_VERSION=1.0.0.0
set EXIT_CODE=0

:ParseCommandLine
if "%1" == "" GOTO EndParseCommandLine
if "%1" == "--system" (
  set REG_HIVE=HKLM
  shift
  GOTO ParseCommandLine
)
if "%1" == "--appid" (
  set APPID=%2
  shift
  shift
  GOTO ParseCommandLine
)
if "%1" == "--company" (
  set COMPANY=%2
  shift
  shift
  GOTO ParseCommandLine
)
if "%1" == "--product_version" (
  set PRODUCT_VERSION=%2
  shift
  shift
  GOTO ParseCommandLine
)
if "%1" == "--exit_code" (
  set EXIT_CODE=%2
  shift
  shift
  GOTO ParseCommandLine
)

shift
GOTO ParseCommandLine

:EndParseCommandLine

reg.exe add %REG_HIVE%\Software\%COMPANY%\Update\Clients\%APPID% /v pv /t REG_SZ /d %PRODUCT_VERSION% /f /reg:32
EXIT %EXIT_CODE%
