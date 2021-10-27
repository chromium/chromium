// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_VIRTUAL_DRIVER_WIN_PORT_MONITOR_SPOOLER_WIN_H_
#define CLOUD_PRINT_VIRTUAL_DRIVER_WIN_PORT_MONITOR_SPOOLER_WIN_H_

#include <windows.h>

// Compatible structures and prototypes are also defined in the Windows DDK in
// winsplp.h.
#ifndef _WINSPLP_

typedef struct {
  DWORD size;
  BOOL(WINAPI* pfnEnumPorts)
  (HANDLE,
   wchar_t*,
   DWORD level,
   BYTE* ports,
   DWORD ports_size,
   DWORD* needed_bytes,
   DWORD* returned);

  BOOL(WINAPI* pfnOpenPort)(HANDLE monitor_data, wchar_t*, HANDLE* handle);

  void* pfnOpenPortEx;  // Unused.

  BOOL(WINAPI* pfnStartDocPort)
  (HANDLE port_handle, wchar_t* printer_name, DWORD job_id, DWORD, BYTE*);

  BOOL(WINAPI* pfnWritePort)
  (HANDLE port, BYTE* buffer, DWORD buffer_size, DWORD* bytes_written);

  BOOL(WINAPI* pfnReadPort)(HANDLE, BYTE*, DWORD, DWORD* bytes_read);

  BOOL(WINAPI* pfnEndDocPort)(HANDLE port_handle);

  BOOL(WINAPI* pfnClosePort)(HANDLE port_handle);

  void* pfnAddPort;  // Unused.

  void* pfnAddPortEx;  // Unused.

  void* pfnConfigurePort;  // Unused.

  void* pfnDeletePort;  // Unused.

  void* pfnGetPrinterDataFromPort;  // Unused.

  void* pfnSetPortTimeOuts;  // Unusued.

  BOOL(WINAPI* pfnXcvOpenPort)
  (HANDLE monitor, const wchar_t*, ACCESS_MASK granted_access, HANDLE* handle);

  DWORD(WINAPI* pfnXcvDataPort)
  (HANDLE xcv_handle,
   const wchar_t* data_name,
   BYTE*,
   DWORD,
   BYTE* output_data,
   DWORD output_data_bytes,
   DWORD* output_data_bytes_needed);

  BOOL(WINAPI* pfnXcvClosePort)(HANDLE handle);

  VOID(WINAPI* pfnShutdown)(HANDLE monitor_handle);
} MONITOR2;

typedef struct {
  DWORD size;

  BOOL(WINAPI* pfnAddPortUI)
  (const wchar_t*, HWND hwnd, const wchar_t* monitor_name, wchar_t**);

  BOOL(WINAPI* pfnConfigurePortUI)
  (const wchar_t*, HWND hwnd, const wchar_t* port_name);

  BOOL(WINAPI* pfnDeletePortUI)
  (const wchar_t*, HWND hwnd, const wchar_t* port_name);
} MONITORUI;

typedef struct {
  DWORD cbSize;
  HANDLE hSpooler;
  HKEY hckRegistryRoot;
  void* pMonitorReg;  // Unused
  BOOL bLocal;
  LPCWSTR pszServerName;
} MONITORINIT;

MONITOR2* WINAPI InitializePrintMonitor2(MONITORINIT* monitor_init,
                                         HANDLE* monitor_handle);

MONITORUI* WINAPI InitializePrintMonitorUI(void);

#endif  // ifdef USE_WIN_DDK
#endif  // CLOUD_PRINT_VIRTUAL_DRIVER_WIN_PORT_MONITOR_SPOOLER_WIN_H_
