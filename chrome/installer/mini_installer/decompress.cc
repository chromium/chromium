// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/mini_installer/decompress.h"

#include <windows.h>

#include <fcntl.h>  // for _O_* constants
#include <fdi.h>
#include <stddef.h>
#include <stdlib.h>

#include "chrome/installer/mini_installer/mini_file.h"

namespace {

// A simple struct to hold data passed to and from FDICopy via its |pvUser|
// argument.
struct ExpandContext {
  // The path to the single destination file.
  const wchar_t* const dest_path;

  // The destination file; valid once the destination is created.
  mini_installer::MiniFile dest_file;

  // Set to true if the file was extracted to |dest_path|. Note that |dest_file|
  // may be valid even in case of failure.
  bool succeeded;
};

FNALLOC(Alloc) {
  return ::HeapAlloc(::GetProcessHeap(), 0, cb);
}

FNFREE(Free) {
  ::HeapFree(::GetProcessHeap(), 0, pv);
}

// Converts a wide string to utf8.  Set |len| to -1 if |str| is zero terminated
// and you want to convert the entire string.
// The returned string will have been allocated with Alloc(), so free it
// with a call to Free().
char* WideToUtf8(const wchar_t* str, int len) {
  char* ret = nullptr;
  int size =
      WideCharToMultiByte(CP_UTF8, 0, str, len, nullptr, 0, nullptr, nullptr);
  if (size) {
    if (len != -1)
      ++size;  // include space for the terminator.
    ret = reinterpret_cast<char*>(Alloc(size * sizeof(ret[0])));
    if (ret) {
      WideCharToMultiByte(CP_UTF8, 0, str, len, ret, size, nullptr, nullptr);
      if (len != -1)
        ret[size - 1] = '\0';  // terminate the string
    }
  }
  return ret;
}

wchar_t* Utf8ToWide(const char* str) {
  wchar_t* ret = nullptr;
  int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
  if (size) {
    ret = reinterpret_cast<wchar_t*>(Alloc(size * sizeof(ret[0])));
    if (ret)
      MultiByteToWideChar(CP_UTF8, 0, str, -1, ret, size);
  }
  return ret;
}

template <typename T>
class scoped_ptr {
 public:
  explicit scoped_ptr(T* a) : a_(a) {}
  ~scoped_ptr() {
    if (a_)
      Free(a_);
  }
  operator T*() { return a_; }

 private:
  T* a_;
};

FNOPEN(Open) {
  DWORD access = 0;
  DWORD disposition = 0;

  if (oflag & _O_RDWR) {
    access = GENERIC_READ | GENERIC_WRITE;
  } else if (oflag & _O_WRONLY) {
    access = GENERIC_WRITE;
  } else {
    access = GENERIC_READ;
  }

  if (oflag & _O_CREAT) {
    disposition = CREATE_ALWAYS;
  } else {
    disposition = OPEN_EXISTING;
  }

  scoped_ptr<wchar_t> path(Utf8ToWide(pszFile));
  HANDLE file =
      CreateFileW(path, access, FILE_SHARE_DELETE | FILE_SHARE_READ, nullptr,
                  disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
  return reinterpret_cast<INT_PTR>(file);
}

FNREAD(Read) {
  DWORD read = 0;
  if (!::ReadFile(reinterpret_cast<HANDLE>(hf), pv, cb, &read, nullptr))
    read = static_cast<DWORD>(-1L);
  return read;
}

FNWRITE(Write) {
  DWORD written = 0;
  if (!::WriteFile(reinterpret_cast<HANDLE>(hf), pv, cb, &written, nullptr))
    written = static_cast<DWORD>(-1L);
  return written;
}

FNCLOSE(Close) {
  return ::CloseHandle(reinterpret_cast<HANDLE>(hf)) ? 0 : -1;
}

FNSEEK(Seek) {
  return ::SetFilePointer(reinterpret_cast<HANDLE>(hf), dist, nullptr,
                          seektype);
}

FNFDINOTIFY(Notify) {
  // Since we will only ever be decompressing a single file at a time
  // we take a shortcut and provide a pointer to the wide destination file
  // of the file we want to write.  This way we don't have to bother with
  // utf8/wide conversion and concatenation of directory and file name.
  ExpandContext& context = *reinterpret_cast<ExpandContext*>(pfdin->pv);

  switch (fdint) {
    case fdintCOPY_FILE:
      context.dest_file.Create(context.dest_path);
      // By sheer coincidence, CreateFileW's success/failure results match that
      // of fdintCOPY_FILE. The handle given out here is closed either by
      // FDICopy (in case of error) or below when handling fdintCLOSE_FILE_INFO
      // (in case of success).
      return reinterpret_cast<INT_PTR>(context.dest_file.DuplicateHandle());

    case fdintCLOSE_FILE_INFO: {
      // Set the file's creation time and file attributes.
      FILE_BASIC_INFO info = {};
      FILETIME file_time;
      FILETIME local;
      if (DosDateTimeToFileTime(pfdin->date, pfdin->time, &file_time) &&
          LocalFileTimeToFileTime(&file_time, &local)) {
        info.CreationTime.u.LowPart = local.dwLowDateTime;
        info.CreationTime.u.HighPart = local.dwHighDateTime;
      }
      info.FileAttributes =
          pfdin->attribs & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN |
                            FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE);
      ::SetFileInformationByHandle(reinterpret_cast<HANDLE>(pfdin->hf),
                                   FileBasicInfo, &info, sizeof(info));

      // Close the handle given out above in fdintCOPY_FILE.
      ::CloseHandle(reinterpret_cast<HANDLE>(pfdin->hf));
      context.succeeded = true;
      return -1;  // Break: the one file was extracted.
    }

    case fdintCABINET_INFO:
    case fdintENUMERATE:
      return 0;  // Continue: success.

    case fdintPARTIAL_FILE:
    case fdintNEXT_CABINET:
    default:
      return -1;  // Break: error.
  }
}

// Module handle of cabinet.dll
HMODULE g_fdi = nullptr;

// API prototypes.
typedef HFDI(DIAMONDAPI* FDICreateFn)(PFNALLOC alloc,
                                      PFNFREE free,
                                      PFNOPEN open,
                                      PFNREAD read,
                                      PFNWRITE write,
                                      PFNCLOSE close,
                                      PFNSEEK seek,
                                      int cpu_type,
                                      PERF perf);
typedef BOOL(DIAMONDAPI* FDIDestroyFn)(HFDI fdi);
typedef BOOL(DIAMONDAPI* FDICopyFn)(HFDI fdi,
                                    char* cab,
                                    char* cab_path,
                                    int flags,
                                    PFNFDINOTIFY notify,
                                    PFNFDIDECRYPT decrypt,
                                    void* context);
FDICreateFn g_FDICreate = nullptr;
FDIDestroyFn g_FDIDestroy = nullptr;
FDICopyFn g_FDICopy = nullptr;

bool InitializeFdi() {
  if (!g_fdi) {
    // It has been observed that some users do not have the expected
    // environment variables set, so we try a couple that *should* always be
    // present and fallback to the default Windows install path if all else
    // fails.
    // The cabinet.dll should be available on all supported versions of Windows.
    static const wchar_t* const candidate_paths[] = {
        L"%WINDIR%\\system32\\cabinet.dll",
        L"%SYSTEMROOT%\\system32\\cabinet.dll",
        L"C:\\Windows\\system32\\cabinet.dll",
    };

    wchar_t path[MAX_PATH] = {0};
    for (size_t i = 0; i < _countof(candidate_paths); ++i) {
      path[0] = L'\0';
      DWORD result =
          ::ExpandEnvironmentStringsW(candidate_paths[i], path, _countof(path));

      if (result > 0 && result <= _countof(path))
        g_fdi = ::LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);

      if (g_fdi)
        break;
    }
  }

  if (g_fdi) {
    g_FDICreate =
        reinterpret_cast<FDICreateFn>(::GetProcAddress(g_fdi, "FDICreate"));
    g_FDIDestroy =
        reinterpret_cast<FDIDestroyFn>(::GetProcAddress(g_fdi, "FDIDestroy"));
    g_FDICopy = reinterpret_cast<FDICopyFn>(::GetProcAddress(g_fdi, "FDICopy"));
  }

  return g_FDICreate && g_FDIDestroy && g_FDICopy;
}

}  // namespace

namespace mini_installer {

bool Expand(const wchar_t* source, const wchar_t* destination) {
  if (!InitializeFdi())
    return false;

  // Start by splitting up the source path and convert to utf8 since the
  // cabinet API doesn't support wide strings.
  const wchar_t* source_name = source + lstrlenW(source);
  while (source_name > source && *source_name != L'\\')
    --source_name;
  if (source_name == source)
    return false;

  // Convert the name to utf8.
  source_name++;
  scoped_ptr<char> source_name_utf8(WideToUtf8(source_name, -1));
  // The directory part is assumed to have a trailing backslash.
  scoped_ptr<char> source_path_utf8(WideToUtf8(source, source_name - source));

  if (!source_name_utf8 || !source_path_utf8)
    return false;

  ERF erf = {0};
  HFDI fdi = g_FDICreate(&Alloc, &Free, &Open, &Read, &Write, &Close, &Seek,
                         cpuUNKNOWN, &erf);
  if (!fdi)
    return false;

  ExpandContext context = {destination, {}, /*succeeded=*/false};
  g_FDICopy(fdi, source_name_utf8, source_path_utf8, 0, &Notify, nullptr,
            &context);
  g_FDIDestroy(fdi);
  if (context.succeeded) {
    // https://crbug.com/1443320: We see crashes on Windows 10 when running
    // setup.exe in which it appears that an entire hunk of the file is zeros or
    // random memory. There is nothing out of the ordinary in the way that this
    // file is written (0x8000 byte chunks via normal WriteFile calls). As an
    // experiment, flush the file before closing it.
    ::FlushFileBuffers(context.dest_file.GetHandleUnsafe());
    return true;
  }

  // Delete the output file if it was created.
  if (context.dest_file.IsValid())
    context.dest_file.DeleteOnClose();

  return false;
}

}  // namespace mini_installer
