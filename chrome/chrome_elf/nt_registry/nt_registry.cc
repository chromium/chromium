// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/nt_registry/nt_registry.h"

#include <assert.h>
#include <stdlib.h>

#include <memory>
#include <string>

namespace {

// Function pointers used for registry access.
RtlInitUnicodeStringFunction g_rtl_init_unicode_string = nullptr;
NtCreateKeyFunction g_nt_create_key = nullptr;
NtDeleteKeyFunction g_nt_delete_key = nullptr;
NtOpenKeyExFunction g_nt_open_key_ex = nullptr;
NtCloseFunction g_nt_close = nullptr;
NtQueryKeyFunction g_nt_query_key = nullptr;
NtEnumerateKeyFunction g_nt_enumerate_key = nullptr;
NtQueryValueKeyFunction g_nt_query_value_key = nullptr;
NtSetValueKeyFunction g_nt_set_value_key = nullptr;

// Lazy init.  No concern about concurrency in chrome_elf.
bool g_initialized = false;
bool g_system_install = false;
bool g_wow64_proc = false;
wchar_t g_kRegPathHKLM[] = L"\\Registry\\Machine\\";
wchar_t g_kRegPathHKCU[nt::g_kRegMaxPathLen + 1] = L"";
wchar_t g_current_user_sid_string[nt::g_kRegMaxPathLen + 1] = L"";

// Max number of tries for system API calls when STATUS_BUFFER_OVERFLOW or
// STATUS_BUFFER_TOO_SMALL can be returned.
enum { kMaxTries = 5 };

// For testing only.
wchar_t g_HKLM_override[nt::g_kRegMaxPathLen + 1] = L"";
wchar_t g_HKCU_override[nt::g_kRegMaxPathLen + 1] = L"";

//------------------------------------------------------------------------------
// Initialization - LOCAL
//------------------------------------------------------------------------------

// Not using install_static, to prevent circular dependency.
bool IsThisProcSystem() {
  wchar_t program_dir[MAX_PATH] = {};
  wchar_t* cmd_line = GetCommandLineW();
  // If our command line starts with the "Program Files" or
  // "Program Files (x86)" path, we're system.
  DWORD ret = ::GetEnvironmentVariable(L"PROGRAMFILES", program_dir, MAX_PATH);
  if (ret && ret < MAX_PATH && !::wcsncmp(cmd_line, program_dir, ret))
    return true;

  ret = ::GetEnvironmentVariable(L"PROGRAMFILES(X86)", program_dir, MAX_PATH);
  if (ret && ret < MAX_PATH && !::wcsncmp(cmd_line, program_dir, ret))
    return true;

  return false;
}

bool IsThisProcWow64() {
  // Using BOOL type for compat with IsWow64Process() system API.
  BOOL is_wow64 = FALSE;

  // API might not exist, so dynamic lookup.
  using IsWow64ProcessFunction = decltype(&IsWow64Process);
  IsWow64ProcessFunction is_wow64_process =
      reinterpret_cast<IsWow64ProcessFunction>(::GetProcAddress(
          ::GetModuleHandle(L"kernel32.dll"), "IsWow64Process"));
  if (!is_wow64_process)
    return false;
  if (!is_wow64_process(::GetCurrentProcess(), &is_wow64))
    return false;
  return is_wow64 ? true : false;
}

bool InitNativeRegApi() {
  HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");

  // Setup the global function pointers for registry access.
  g_rtl_init_unicode_string = reinterpret_cast<RtlInitUnicodeStringFunction>(
      ::GetProcAddress(ntdll, "RtlInitUnicodeString"));

  g_nt_create_key = reinterpret_cast<NtCreateKeyFunction>(
      ::GetProcAddress(ntdll, "NtCreateKey"));

  g_nt_delete_key = reinterpret_cast<NtDeleteKeyFunction>(
      ::GetProcAddress(ntdll, "NtDeleteKey"));

  g_nt_open_key_ex = reinterpret_cast<NtOpenKeyExFunction>(
      ::GetProcAddress(ntdll, "NtOpenKeyEx"));

  g_nt_close =
      reinterpret_cast<NtCloseFunction>(::GetProcAddress(ntdll, "NtClose"));

  g_nt_query_key = reinterpret_cast<NtQueryKeyFunction>(
      ::GetProcAddress(ntdll, "NtQueryKey"));

  g_nt_enumerate_key = reinterpret_cast<NtEnumerateKeyFunction>(
      ::GetProcAddress(ntdll, "NtEnumerateKey"));

  g_nt_query_value_key = reinterpret_cast<NtQueryValueKeyFunction>(
      ::GetProcAddress(ntdll, "NtQueryValueKey"));

  g_nt_set_value_key = reinterpret_cast<NtSetValueKeyFunction>(
      ::GetProcAddress(ntdll, "NtSetValueKey"));

  if (!g_rtl_init_unicode_string || !g_nt_create_key || !g_nt_open_key_ex ||
      !g_nt_delete_key || !g_nt_close || !g_nt_query_key ||
      !g_nt_enumerate_key || !g_nt_query_value_key || !g_nt_set_value_key)
    return false;

  // We need to set HKCU based on the sid of the current user account.
  RtlFormatCurrentUserKeyPathFunction rtl_current_user_string =
      reinterpret_cast<RtlFormatCurrentUserKeyPathFunction>(
          ::GetProcAddress(ntdll, "RtlFormatCurrentUserKeyPath"));

  RtlFreeUnicodeStringFunction rtl_free_unicode_str =
      reinterpret_cast<RtlFreeUnicodeStringFunction>(
          ::GetProcAddress(ntdll, "RtlFreeUnicodeString"));

  if (!rtl_current_user_string || !rtl_free_unicode_str)
    return false;

  UNICODE_STRING current_user_reg_path;
  if (!NT_SUCCESS(rtl_current_user_string(&current_user_reg_path)))
    return false;

  // Finish setting up global HKCU path.
  ::wcsncat(g_kRegPathHKCU, current_user_reg_path.Buffer, nt::g_kRegMaxPathLen);
  ::wcsncat(g_kRegPathHKCU, L"\\",
            (nt::g_kRegMaxPathLen - ::wcslen(g_kRegPathHKCU)));
  // Keep the sid string as well.
  wchar_t* ptr = ::wcsrchr(current_user_reg_path.Buffer, L'\\');
  ptr++;
  ::wcsncpy(g_current_user_sid_string, ptr, nt::g_kRegMaxPathLen);
  rtl_free_unicode_str(&current_user_reg_path);

  // Figure out if this is a system or user install.
  g_system_install = IsThisProcSystem();

  // Figure out if this is a WOW64 process.
  g_wow64_proc = IsThisProcWow64();

  g_initialized = true;
  return true;
}

//------------------------------------------------------------------------------
// Reg WOW64 Redirection - LOCAL
//
// How registry redirection works directly calling NTDLL APIs:
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// - NOTE: On >= Win7, reflection support was removed.
// -
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa384253(v=vs.85).aspx
//
// - 1) 32-bit / WOW64 process:
//     a) Default access WILL be redirected to WOW64.
//     b) KEY_WOW64_32KEY access WILL be redirected to WOW64.
//     c) KEY_WOW64_64KEY access will NOT be redirected to WOW64.
//
// - 2) 64-bit process:
//     a) Default access will NOT be redirected to WOW64.
//     b) KEY_WOW64_32KEY access will NOT be redirected to WOW64.
//     c) KEY_WOW64_64KEY access will NOT be redirected to WOW64.
//
// - Key point from above is that NTDLL redirects and respects access
//   overrides for WOW64 calling processes.  But does NOT do any of that if the
//   calling process is 64-bit.  2b is surprising and troublesome.
//
// How registry redirection works using these nt_registry APIs:
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// - These APIs will behave the same as NTDLL above, EXCEPT for 2b.
//   nt_registry APIs will respect the override access flags for all processes.
//
// - How the WOW64 redirection decision trees / Nodes work below:
//
//   The HKLM and HKCU decision trees represent the information at the MSDN
//   link above... but in a way that generates a decision about whether a
//   registry path should be subject to WOW64 redirection.  The tree is
//   traversed as you scan along the registry path in question.
//
//    - Each Node contains a chunk of registry subkey(s) to match.
//    - If it is NOT matched, traversal is done.
//    - If it is matched:
//       - Current state of |redirection_type| for the whole registry path is
//         updated.
//       - If |next| is empty, traversal is done.
//       - Otherwise, |next| is an array of child Nodes to try to match against.
//         Loop.
//------------------------------------------------------------------------------

// This enum defines states for how to handle redirection.
// NOTE: When WOW64 redirection should happen, the redirect subkey can be either
//       before or after the latest Node match.  Unfortunately not consistent.
enum RedirectionType { SHARED = 0, REDIRECTED_BEFORE, REDIRECTED_AFTER };

struct Node {
  template <size_t len, size_t n_len>
  constexpr Node(const wchar_t (&wcs)[len],
                 RedirectionType rt,
                 const Node (&n)[n_len])
      : to_match(wcs),
        to_match_len(len - 1),
        redirection_type(rt),
        next(n),
        next_len(n_len) {}

  template <size_t len>
  constexpr Node(const wchar_t (&wcs)[len], RedirectionType rt)
      : to_match(wcs),
        to_match_len(len - 1),
        redirection_type(rt),
        next(nullptr),
        next_len(0) {}

  const wchar_t* to_match;
  size_t to_match_len;
  // If a match, this is the new state of how to redirect.
  RedirectionType redirection_type;
  // |next| is nullptr or an array of Nodes of length |array_len|.
  const Node* next;
  size_t next_len;
};

// HKLM or HKCU SOFTWARE\Classes is shared by default.  Specific subkeys under
// Classes are redirected to SOFTWARE\WOW6432Node\Classes\<subkey> though.
constexpr Node kClassesSubtree[] = {{L"CLSID", REDIRECTED_BEFORE},
                                    {L"DirectShow", REDIRECTED_BEFORE},
                                    {L"Interface", REDIRECTED_BEFORE},
                                    {L"Media Type", REDIRECTED_BEFORE},
                                    {L"MediaFoundation", REDIRECTED_BEFORE}};

// These specific HKLM\SOFTWARE subkeys are shared.  Specific
// subkeys under Classes are redirected though... see classes_subtree.
constexpr Node kHklmSoftwareSubtree[] = {
    // TODO(pennymac): when MS fixes compiler bug, or bots are all using clang,
    // remove the "Classes" subkeys below and replace with:
    // {L"Classes", SHARED, kClassesSubtree},
    // https://connect.microsoft.com/VisualStudio/feedback/details/3104499
    {L"Classes\\CLSID", REDIRECTED_BEFORE},
    {L"Classes\\DirectShow", REDIRECTED_BEFORE},
    {L"Classes\\Interface", REDIRECTED_BEFORE},
    {L"Classes\\Media Type", REDIRECTED_BEFORE},
    {L"Classes\\MediaFoundation", REDIRECTED_BEFORE},
    {L"Classes", SHARED},

    {L"Clients", SHARED},
    {L"Microsoft\\COM3", SHARED},
    {L"Microsoft\\Cryptography\\Calais\\Current", SHARED},
    {L"Microsoft\\Cryptography\\Calais\\Readers", SHARED},
    {L"Microsoft\\Cryptography\\Services", SHARED},

    {L"Microsoft\\CTF\\SystemShared", SHARED},
    {L"Microsoft\\CTF\\TIP", SHARED},
    {L"Microsoft\\DFS", SHARED},
    {L"Microsoft\\Driver Signing", SHARED},
    {L"Microsoft\\EnterpriseCertificates", SHARED},

    {L"Microsoft\\EventSystem", SHARED},
    {L"Microsoft\\MSMQ", SHARED},
    {L"Microsoft\\Non-Driver Signing", SHARED},
    {L"Microsoft\\Notepad\\DefaultFonts", SHARED},
    {L"Microsoft\\OLE", SHARED},

    {L"Microsoft\\RAS", SHARED},
    {L"Microsoft\\RPC", SHARED},
    {L"Microsoft\\SOFTWARE\\Microsoft\\Shared Tools\\MSInfo", SHARED},
    {L"Microsoft\\SystemCertificates", SHARED},
    {L"Microsoft\\TermServLicensing", SHARED},

    {L"Microsoft\\Transaction Server", SHARED},
    {L"Microsoft\\Windows\\CurrentVersion\\App Paths", SHARED},
    {L"Microsoft\\Windows\\CurrentVersion\\Control Panel\\Cursors\\Schemes",
     SHARED},
    {L"Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers", SHARED},
    {L"Microsoft\\Windows\\CurrentVersion\\Explorer\\DriveIcons", SHARED},

    {L"Microsoft\\Windows\\CurrentVersion\\Explorer\\KindMap", SHARED},
    {L"Microsoft\\Windows\\CurrentVersion\\Group Policy", SHARED},
    {L"Microsoft\\Windows\\CurrentVersion\\Policies", SHARED},
    {L"Microsoft\\Windows\\CurrentVersion\\PreviewHandlers", SHARED},
    {L"Microsoft\\Windows\\CurrentVersion\\Setup", SHARED},

    {L"Microsoft\\Windows\\CurrentVersion\\Telephony\\Locations", SHARED},
    {L"Microsoft\\Windows NT\\CurrentVersion\\Console", SHARED},
    {L"Microsoft\\Windows NT\\CurrentVersion\\FontDpi", SHARED},
    {L"Microsoft\\Windows NT\\CurrentVersion\\FontLink", SHARED},
    {L"Microsoft\\Windows NT\\CurrentVersion\\FontMapper", SHARED},

    {L"Microsoft\\Windows NT\\CurrentVersion\\Fonts", SHARED},
    {L"Microsoft\\Windows NT\\CurrentVersion\\FontSubstitutes", SHARED},
    {L"Microsoft\\Windows NT\\CurrentVersion\\Gre_Initialize", SHARED},
    {L"Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options",
     SHARED},
    {L"Microsoft\\Windows NT\\CurrentVersion\\LanguagePack", SHARED},

    {L"Microsoft\\Windows NT\\CurrentVersion\\NetworkCards", SHARED},
    {L"Microsoft\\Windows NT\\CurrentVersion\\Perflib", SHARED},
    {L"Microsoft\\Windows NT\\CurrentVersion\\Ports", SHARED},
    {L"Microsoft\\Windows NT\\CurrentVersion\\Print", SHARED},
    {L"Microsoft\\Windows NT\\CurrentVersion\\ProfileList", SHARED},

    {L"Microsoft\\Windows NT\\CurrentVersion\\Time Zones", SHARED},
    {L"Policies", SHARED},
    {L"RegisteredApplications", SHARED}};

// HKCU is entirely shared, except for a few specific Classes subkeys which
// are redirected.  See |classes_subtree|.
constexpr Node kRedirectionDecisionTreeHkcu = {L"SOFTWARE\\Classes", SHARED,
                                               kClassesSubtree};

// HKLM\SOFTWARE is redirected by default to SOFTWARE\WOW6432Node.  Specific
// subkeys under SOFTWARE are shared though... see |hklm_software_subtree|.
constexpr Node kRedirectionDecisionTreeHklm = {L"SOFTWARE", REDIRECTED_AFTER,
                                               kHklmSoftwareSubtree};

// Main redirection handler function.
// If redirection is required, change is made to |subkey_path| in place.
//
// - This function should be called BEFORE concatenating |subkey_path| with the
//   root hive or calling ParseFullRegPath().
// - Also, |subkey_path| should be passed to SanitizeSubkeyPath() before calling
//   this function.
void ProcessRedirection(nt::ROOT_KEY root,
                        ACCESS_MASK access,
                        std::wstring* subkey_path) {
  static constexpr wchar_t kRedirectBefore[] = L"WOW6432Node\\";
  static constexpr wchar_t kRedirectAfter[] = L"\\WOW6432Node";

  assert(subkey_path != nullptr);
  assert(subkey_path->empty() || subkey_path->front() != L'\\');
  assert(subkey_path->empty() || subkey_path->back() != L'\\');
  assert(root != nt::AUTO);

  // |subkey_path| could legitimately be empty.
  if (subkey_path->empty() ||
      (access & KEY_WOW64_32KEY && access & KEY_WOW64_64KEY))
    return;

  // No redirection during testing when there's already an override.
  // Otherwise, the testing redirect directory Software\Chromium\TempTestKeys
  // would get WOW64 redirected if root_key == HKLM in this function.
  if (root == nt::HKCU ? *g_HKCU_override : *g_HKLM_override)
    return;

  // WOW64 redirection only supported on x64 architecture.  Return if x86.
  SYSTEM_INFO system_info = {};
  ::GetNativeSystemInfo(&system_info);
  if (system_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
    return;

  bool use_wow64 = g_wow64_proc;
  // Consider KEY_WOW64_32KEY and KEY_WOW64_64KEY override access flags.
  if (access & KEY_WOW64_32KEY)
    use_wow64 = true;
  if (access & KEY_WOW64_64KEY)
    use_wow64 = false;

  // If !use_wow64, there's nothing more to do.
  if (!use_wow64)
    return;

  // The root of the decision trees are an array of 1.
  size_t node_array_len = 1;
  // Pick which decision tree to use.
  const Node* current_node = (root == nt::HKCU) ? &kRedirectionDecisionTreeHkcu
                                                : &kRedirectionDecisionTreeHklm;

  // The following loop works on the |subkey_path| from left to right.
  // |position| tracks progress along |subkey_path|.
  const wchar_t* position = subkey_path->c_str();
  // Hold a count of chars left after position, for efficient calculations.
  size_t chars_left = subkey_path->length();
  // |redirect_state| holds the latest state of redirection requirement.
  RedirectionType redirect_state = SHARED;
  // |insertion_point| tracks latest spot for redirection subkey to be inserted.
  const wchar_t* insertion_point = nullptr;
  // |insert_string| tracks which redirection string would be inserted.
  const wchar_t* insert_string = nullptr;

  size_t node_index = 0;
  while (node_index < node_array_len) {
    size_t current_to_match_len = current_node->to_match_len;
    // Make sure the remainder of the path is at least as long as the current
    // subkey to match.
    if (chars_left >= current_to_match_len) {
      // Do case insensitive comparisons.
      if (!::wcsnicmp(position, current_node->to_match, current_to_match_len)) {
        // Make sure not to match on a substring.
        if (*(position + current_to_match_len) == L'\\' ||
            *(position + current_to_match_len) == L'\0') {
          // MATCH!
          // -------------------------------------------------------------------
          // 1) Update state of redirection.
          redirect_state = current_node->redirection_type;
          // 1.5) If new state is to redirect, the new insertion point will be
          //      either right before or right after this match.
          if (redirect_state == REDIRECTED_BEFORE) {
            insertion_point = position;
            insert_string = kRedirectBefore;
          } else if (redirect_state == REDIRECTED_AFTER) {
            insertion_point = position + current_to_match_len;
            insert_string = kRedirectAfter;
          }
          // 2) Adjust |position| along the subkey path.
          position += current_to_match_len;
          chars_left -= current_to_match_len;
          // 2.5) Increment the position, to move past path seperator(s).
          while (*position == L'\\') {
            ++position;
            --chars_left;
          }
          // 3) Move our loop parameters to the |next| array of Nodes.
          node_array_len = current_node->next_len;
          current_node = current_node->next;
          node_index = 0;
          // 4) Finish this loop and start on new array.
          continue;
        }
      }
    }

    // Move to the next node in the array if we didn't match this loop.
    ++current_node;
    ++node_index;
  }

  if (redirect_state == SHARED)
    return;

  // Insert the redirection into |subkey_path|, at |insertion_point|.
  subkey_path->insert((insertion_point - subkey_path->c_str()), insert_string);
}

//------------------------------------------------------------------------------
// Reg Path Utilities - LOCAL
//------------------------------------------------------------------------------

std::wstring ConvertRootKey(nt::ROOT_KEY root) {
  assert(root != nt::AUTO);

  if (root == nt::HKCU && *g_HKCU_override) {
    std::wstring temp = g_kRegPathHKCU;
    temp.append(g_HKCU_override);
    temp.append(L"\\");
    return temp;
  } else if (root == nt::HKLM && *g_HKLM_override) {
    // Yes, HKLM override goes into HKCU.  This is not a typo.
    std::wstring temp = g_kRegPathHKCU;
    temp.append(g_HKLM_override);
    temp.append(L"\\");
    return temp;
  }

  return (root == nt::HKCU) ? g_kRegPathHKCU : g_kRegPathHKLM;
}

// This utility should be called on an externally provided subkey path.
// - Ensures there are no starting or trailing backslashes, and no more than
// - one backslash in a row.
// - Note from MSDN: "Key names cannot include the backslash character (\),
//   but any other printable character can be used.  Value names and data can
//   include the backslash character."
void SanitizeSubkeyPath(std::wstring* input) {
  assert(input != nullptr);

  // Remove trailing backslashes.
  size_t last_valid_pos = input->find_last_not_of(L'\\');
  if (last_valid_pos == std::wstring::npos) {
    // The string is all backslashes, or it's empty.  Clear and abort.
    input->clear();
    return;
  }
  // Chop off the trailing backslashes.
  input->resize(last_valid_pos + 1);

  // Remove leading backslashes.
  input->erase(0, input->find_first_not_of(L'\\'));

  // Replace any occurances of more than 1 backslash in a row with just 1.
  size_t index = input->find_first_of(L"\\");
  while (index != std::wstring::npos) {
    // Remove a second consecutive backslash, and leave index where it is,
    // or move to the next backslash in the string.
    if ((*input)[index + 1] == L'\\')
      input->erase(index + 1, 1);
    else
      index = input->find_first_of(L"\\", index + 1);
  }
}

// Turns a root and subkey path into the registry base hive and the rest of the
// subkey tokens.
// - |converted_root| should come directly out of ConvertRootKey function.
// - |subkey_path| should be passed to SanitizeSubkeyPath() first.
// - E.g. base hive: "\Registry\Machine\", "\Registry\User\<SID>\".
bool ParseFullRegPath(const std::wstring& converted_root,
                      const std::wstring& subkey_path,
                      std::wstring* out_base,
                      std::vector<std::wstring>* subkeys) {
  out_base->clear();
  subkeys->clear();
  std::wstring temp_path;

  // Special case if there is testing redirection set up.
  if (*g_HKCU_override || *g_HKLM_override) {
    // Why process |converted_root|?  To handle reg redirection used by tests.
    // E.g.:
    // |converted_root| = "\REGISTRY\USER\S-1-5-21-39260824-743453154-142223018-
    // 716772\Software\Chromium\TempTestKeys\13110669370890870$94c6ed9d-bc34-
    // 44f3-a0b3-9eee2d3f2f82\".
    // |subkey_path| = "SOFTWARE\Google\Chrome\BrowserSec".
    //
    // Note: bypassing the starting backslash in the |converted_root|.
    temp_path.append(converted_root, 1, converted_root.size() - 1);
  }
  temp_path.append(subkey_path);

  // Tokenize the full path.
  size_t find_start = 0;
  size_t delimiter = temp_path.find_first_of(L'\\');
  while (delimiter != std::wstring::npos) {
    subkeys->emplace_back(temp_path, find_start, delimiter - find_start);
    // Move past the backslash.
    find_start = delimiter + 1;
    delimiter = temp_path.find_first_of(L'\\', find_start);
  }
  // Get the last token if there is one.
  if (!temp_path.empty())
    subkeys->emplace_back(temp_path, find_start);

  // Special case if there is testing redirection set up.
  if (*g_HKCU_override || *g_HKLM_override) {
    // The base hive for HKCU needs to include the user SID.
    uint32_t num_base_tokens = 2;
    if (0 == temp_path.compare(0, 14, L"REGISTRY\\USER\\"))
      num_base_tokens = 3;

    if (subkeys->size() < num_base_tokens)
      return false;

    // Pull out the base hive tokens.
    out_base->push_back(L'\\');
    for (size_t i = 0; i < num_base_tokens; ++i) {
      out_base->append((*subkeys)[i]);
      out_base->push_back(L'\\');
    }
    subkeys->erase(subkeys->begin(), subkeys->begin() + num_base_tokens);
  } else {
    out_base->assign(converted_root);
  }

  return true;
}

// String safety.
// - NOTE: only working with wchar_t here.
// - Also ensures the content of |value_bytes| is at least a terminator.
// - Pass "true" for |multi| for MULTISZ.
void EnsureTerminatedSZ(std::vector<BYTE>* value_bytes, bool multi) {
  DWORD terminator_size = sizeof(wchar_t);

  if (multi)
    terminator_size = 2 * sizeof(wchar_t);

  // Ensure content is at least the size of a terminator.
  if (value_bytes->size() < terminator_size) {
    value_bytes->insert(value_bytes->end(),
                        terminator_size - value_bytes->size(), 0);
  }

  // Sanity check content size based on character size.
  DWORD modulo = value_bytes->size() % sizeof(wchar_t);
  value_bytes->insert(value_bytes->end(), modulo, 0);

  // Now finally check for trailing terminator.
  bool terminated = true;
  size_t last_element = value_bytes->size() - 1;
  for (size_t i = 0; i < terminator_size; i++) {
    if ((*value_bytes)[last_element - i] != 0) {
      terminated = false;
      break;
    }
  }

  if (terminated)
    return;

  // Append a full terminator to be safe.
  value_bytes->insert(value_bytes->end(), terminator_size, 0);

  return;
}

//------------------------------------------------------------------------------
// Misc wrapper functions - LOCAL
//------------------------------------------------------------------------------

NTSTATUS CreateKeyWrapper(const std::wstring& key_path,
                          ACCESS_MASK access,
                          HANDLE* out_handle,
                          ULONG* create_or_open OPTIONAL) {
  UNICODE_STRING key_path_uni = {};
  g_rtl_init_unicode_string(&key_path_uni, key_path.c_str());

  OBJECT_ATTRIBUTES obj = {};
  InitializeObjectAttributes(&obj, &key_path_uni, OBJ_CASE_INSENSITIVE, NULL,
                             nullptr);

  return g_nt_create_key(out_handle, access, &obj, 0, nullptr,
                         REG_OPTION_NON_VOLATILE, create_or_open);
}

}  // namespace

namespace nt {

//------------------------------------------------------------------------------
// Create, open, delete, close functions
//------------------------------------------------------------------------------

bool CreateRegKey(ROOT_KEY root,
                  const wchar_t* key_path,
                  ACCESS_MASK access,
                  HANDLE* out_handle OPTIONAL) {
  // |key_path| can be null or empty, but it can't be longer than
  // |g_kRegMaxPathLen| at this point.
  if (key_path != nullptr &&
      ::wcsnlen(key_path, g_kRegMaxPathLen + 1) == g_kRegMaxPathLen + 1)
    return false;

  if (!g_initialized && !InitNativeRegApi())
    return false;

  if (root == nt::AUTO)
    root = g_system_install ? nt::HKLM : nt::HKCU;

  std::wstring redirected_key_path;
  if (key_path) {
    redirected_key_path = key_path;
    SanitizeSubkeyPath(&redirected_key_path);
    ProcessRedirection(root, access, &redirected_key_path);
  }

  std::wstring current_path;
  std::vector<std::wstring> subkeys;
  if (!ParseFullRegPath(ConvertRootKey(root), redirected_key_path,
                        &current_path, &subkeys))
    return false;

  // Open the base hive first.  It should always exist already.
  HANDLE last_handle = INVALID_HANDLE_VALUE;
  NTSTATUS status =
      CreateKeyWrapper(current_path, access, &last_handle, nullptr);
  if (!NT_SUCCESS(status))
    return false;

  size_t subkeys_size = subkeys.size();
  if (subkeys_size != 0)
    g_nt_close(last_handle);

  // Recursively open/create each subkey.
  std::vector<HANDLE> rollback;
  bool success = true;
  for (size_t i = 0; i < subkeys_size; i++) {
    current_path.append(subkeys[i]);
    current_path.push_back(L'\\');

    // Process the latest subkey.
    ULONG created = 0;
    HANDLE key_handle = INVALID_HANDLE_VALUE;
    status =
        CreateKeyWrapper(current_path.c_str(), access, &key_handle, &created);
    if (!NT_SUCCESS(status)) {
      success = false;
      break;
    }

    if (i == subkeys_size - 1) {
      last_handle = key_handle;
    } else {
      // Save any subkey handle created, in case of rollback.
      if (created == REG_CREATED_NEW_KEY)
        rollback.push_back(key_handle);
      else
        g_nt_close(key_handle);
    }
  }

  if (!success) {
    // Delete any subkeys created.
    for (HANDLE handle : rollback) {
      g_nt_delete_key(handle);
    }
  }
  for (HANDLE handle : rollback) {
    // Close the rollback handles, on success or failure.
    g_nt_close(handle);
  }
  if (!success)
    return false;

  // See if caller wants the handle left open.
  if (out_handle)
    *out_handle = last_handle;
  else
    g_nt_close(last_handle);

  return true;
}

bool OpenRegKey(ROOT_KEY root,
                const wchar_t* key_path,
                ACCESS_MASK access,
                HANDLE* out_handle,
                NTSTATUS* error_code OPTIONAL) {
  // |key_path| can be null or empty, but it can't be longer than
  // |g_kRegMaxPathLen| at this point.
  if (key_path != nullptr &&
      ::wcsnlen(key_path, g_kRegMaxPathLen + 1) == g_kRegMaxPathLen + 1)
    return false;

  if (!g_initialized && !InitNativeRegApi())
    return false;

  NTSTATUS status = STATUS_UNSUCCESSFUL;
  UNICODE_STRING key_path_uni = {};
  OBJECT_ATTRIBUTES obj = {};
  *out_handle = INVALID_HANDLE_VALUE;

  if (root == nt::AUTO)
    root = g_system_install ? nt::HKLM : nt::HKCU;

  std::wstring full_path;
  if (key_path) {
    full_path = key_path;
    SanitizeSubkeyPath(&full_path);
    ProcessRedirection(root, access, &full_path);
  }
  full_path.insert(0, ConvertRootKey(root));

  g_rtl_init_unicode_string(&key_path_uni, full_path.c_str());
  InitializeObjectAttributes(&obj, &key_path_uni, OBJ_CASE_INSENSITIVE, NULL,
                             NULL);

  status = g_nt_open_key_ex(out_handle, access, &obj, 0);
  // See if caller wants the NTSTATUS.
  if (error_code)
    *error_code = status;

  if (NT_SUCCESS(status))
    return true;

  return false;
}

bool DeleteRegKey(HANDLE key) {
  if (!g_initialized && !InitNativeRegApi())
    return false;

  NTSTATUS status = g_nt_delete_key(key);

  return NT_SUCCESS(status);
}

// wrapper function
bool DeleteRegKey(ROOT_KEY root,
                  WOW64_OVERRIDE wow64_override,
                  const wchar_t* key_path) {
  HANDLE key = INVALID_HANDLE_VALUE;

  if (!OpenRegKey(root, key_path, DELETE | wow64_override, &key, nullptr))
    return false;

  if (!DeleteRegKey(key)) {
    CloseRegKey(key);
    return false;
  }

  CloseRegKey(key);
  return true;
}

void CloseRegKey(HANDLE key) {
  if (!g_initialized)
    InitNativeRegApi();
  g_nt_close(key);
}

//------------------------------------------------------------------------------
// Getter functions
//------------------------------------------------------------------------------

bool QueryRegKeyValue(HANDLE key,
                      const wchar_t* value_name,
                      ULONG* out_type,
                      std::vector<BYTE>* out_buffer) {
  if (!g_initialized && !InitNativeRegApi())
    return false;

  UNICODE_STRING value_uni = {};
  g_rtl_init_unicode_string(&value_uni, value_name);

  // Use a loop here, to be a little more tolerant of concurrent registry
  // changes.
  NTSTATUS ntstatus = STATUS_UNSUCCESSFUL;
  int tries = 0;
  KEY_VALUE_FULL_INFORMATION* value_info = nullptr;
  DWORD size_needed = sizeof(*value_info);
  std::vector<BYTE> buffer(size_needed);
  do {
    buffer.resize(size_needed);
    value_info = reinterpret_cast<KEY_VALUE_FULL_INFORMATION*>(buffer.data());

    ntstatus = g_nt_query_value_key(key, &value_uni, KeyValueFullInformation,
                                    value_info, size_needed, &size_needed);
  } while ((ntstatus == STATUS_BUFFER_OVERFLOW ||
            ntstatus == STATUS_BUFFER_TOO_SMALL) &&
           ++tries < kMaxTries);

  if (!NT_SUCCESS(ntstatus))
    return false;

  *out_type = value_info->Type;
  DWORD data_size = value_info->DataLength;

  if (data_size) {
    // Move the data into |out_buffer| vector.
    BYTE* data = reinterpret_cast<BYTE*>(value_info) + value_info->DataOffset;
    out_buffer->assign(data, data + data_size);
  } else {
    out_buffer->clear();
  }

  return true;
}

// wrapper function
bool QueryRegValueDWORD(HANDLE key,
                        const wchar_t* value_name,
                        DWORD* out_dword) {
  ULONG type = REG_NONE;
  std::vector<BYTE> value_bytes;

  if (!QueryRegKeyValue(key, value_name, &type, &value_bytes) ||
      type != REG_DWORD) {
    return false;
  }

  if (value_bytes.size() < sizeof(*out_dword))
    return false;

  *out_dword = *(reinterpret_cast<DWORD*>(value_bytes.data()));

  return true;
}

// wrapper function
bool QueryRegValueDWORD(ROOT_KEY root,
                        WOW64_OVERRIDE wow64_override,
                        const wchar_t* key_path,
                        const wchar_t* value_name,
                        DWORD* out_dword) {
  HANDLE key = INVALID_HANDLE_VALUE;

  if (!OpenRegKey(root, key_path, KEY_QUERY_VALUE | wow64_override, &key, NULL))
    return false;

  if (!QueryRegValueDWORD(key, value_name, out_dword)) {
    CloseRegKey(key);
    return false;
  }

  CloseRegKey(key);
  return true;
}

// wrapper function
bool QueryRegValueSZ(HANDLE key,
                     const wchar_t* value_name,
                     std::wstring* out_sz) {
  std::vector<BYTE> value_bytes;
  ULONG type = REG_NONE;

  if (!QueryRegKeyValue(key, value_name, &type, &value_bytes) ||
      (type != REG_SZ && type != REG_EXPAND_SZ)) {
    return false;
  }

  EnsureTerminatedSZ(&value_bytes, false);

  *out_sz = reinterpret_cast<wchar_t*>(value_bytes.data());

  return true;
}

// wrapper function
bool QueryRegValueSZ(ROOT_KEY root,
                     WOW64_OVERRIDE wow64_override,
                     const wchar_t* key_path,
                     const wchar_t* value_name,
                     std::wstring* out_sz) {
  HANDLE key = INVALID_HANDLE_VALUE;

  if (!OpenRegKey(root, key_path, KEY_QUERY_VALUE | wow64_override, &key, NULL))
    return false;

  if (!QueryRegValueSZ(key, value_name, out_sz)) {
    CloseRegKey(key);
    return false;
  }

  CloseRegKey(key);
  return true;
}

// wrapper function
bool QueryRegValueMULTISZ(HANDLE key,
                          const wchar_t* value_name,
                          std::vector<std::wstring>* out_multi_sz) {
  std::vector<BYTE> value_bytes;
  ULONG type = REG_NONE;

  if (!QueryRegKeyValue(key, value_name, &type, &value_bytes) ||
      type != REG_MULTI_SZ) {
    return false;
  }

  EnsureTerminatedSZ(&value_bytes, true);

  // Make sure the out vector is empty to start.
  out_multi_sz->clear();

  wchar_t* pointer = reinterpret_cast<wchar_t*>(value_bytes.data());
  std::wstring temp = pointer;
  // Loop.  Each string is separated by '\0'.  Another '\0' at very end (so 2 in
  // a row).
  while (!temp.empty()) {
    pointer += temp.length() + 1;
    out_multi_sz->push_back(std::move(temp));
    temp = pointer;
  }

  return true;
}

// wrapper function
bool QueryRegValueMULTISZ(ROOT_KEY root,
                          WOW64_OVERRIDE wow64_override,
                          const wchar_t* key_path,
                          const wchar_t* value_name,
                          std::vector<std::wstring>* out_multi_sz) {
  HANDLE key = INVALID_HANDLE_VALUE;

  if (!OpenRegKey(root, key_path, KEY_QUERY_VALUE | wow64_override, &key, NULL))
    return false;

  if (!QueryRegValueMULTISZ(key, value_name, out_multi_sz)) {
    CloseRegKey(key);
    return false;
  }

  CloseRegKey(key);
  return true;
}

//------------------------------------------------------------------------------
// Setter functions
//------------------------------------------------------------------------------

bool SetRegKeyValue(HANDLE key,
                    const wchar_t* value_name,
                    ULONG type,
                    const BYTE* data,
                    DWORD data_size) {
  if (!g_initialized && !InitNativeRegApi())
    return false;

  NTSTATUS ntstatus = STATUS_UNSUCCESSFUL;
  UNICODE_STRING value_uni = {};
  g_rtl_init_unicode_string(&value_uni, value_name);

  BYTE* non_const_data = const_cast<BYTE*>(data);
  ntstatus =
      g_nt_set_value_key(key, &value_uni, 0, type, non_const_data, data_size);

  if (NT_SUCCESS(ntstatus))
    return true;

  return false;
}

// wrapper function
bool SetRegValueDWORD(HANDLE key, const wchar_t* value_name, DWORD value) {
  return SetRegKeyValue(key, value_name, REG_DWORD,
                        reinterpret_cast<BYTE*>(&value), sizeof(value));
}

// wrapper function
bool SetRegValueDWORD(ROOT_KEY root,
                      WOW64_OVERRIDE wow64_override,
                      const wchar_t* key_path,
                      const wchar_t* value_name,
                      DWORD value) {
  HANDLE key = INVALID_HANDLE_VALUE;

  if (!OpenRegKey(root, key_path, KEY_SET_VALUE | wow64_override, &key, NULL))
    return false;

  if (!SetRegValueDWORD(key, value_name, value)) {
    CloseRegKey(key);
    return false;
  }

  return true;
}

// wrapper function
bool SetRegValueSZ(HANDLE key,
                   const wchar_t* value_name,
                   const std::wstring& value) {
  // Make sure the number of bytes in |value|, including EoS, fits in a DWORD.
  if (std::numeric_limits<DWORD>::max() <
      ((value.length() + 1) * sizeof(wchar_t)))
    return false;

  DWORD size = (static_cast<DWORD>((value.length() + 1) * sizeof(wchar_t)));
  return SetRegKeyValue(key, value_name, REG_SZ,
                        reinterpret_cast<const BYTE*>(value.c_str()), size);
}

// wrapper function
bool SetRegValueSZ(ROOT_KEY root,
                   WOW64_OVERRIDE wow64_override,
                   const wchar_t* key_path,
                   const wchar_t* value_name,
                   const std::wstring& value) {
  HANDLE key = INVALID_HANDLE_VALUE;

  if (!OpenRegKey(root, key_path, KEY_SET_VALUE | wow64_override, &key, NULL))
    return false;

  if (!SetRegValueSZ(key, value_name, value)) {
    CloseRegKey(key);
    return false;
  }

  return true;
}

// wrapper function
bool SetRegValueMULTISZ(HANDLE key,
                        const wchar_t* value_name,
                        const std::vector<std::wstring>& values) {
  std::vector<wchar_t> builder;

  for (auto& string : values) {
    // Just in case someone is passing in an illegal empty string
    // (not allowed in REG_MULTI_SZ), ignore it.
    if (!string.empty()) {
      for (const wchar_t& w : string) {
        builder.push_back(w);
      }
      builder.push_back(L'\0');
    }
  }
  // Add second null terminator to end REG_MULTI_SZ.
  builder.push_back(L'\0');
  // Handle rare case where the vector passed in was empty,
  // or only had an empty string.
  if (builder.size() == 1)
    builder.push_back(L'\0');

  if (std::numeric_limits<DWORD>::max() < builder.size())
    return false;

  return SetRegKeyValue(
      key, value_name, REG_MULTI_SZ, reinterpret_cast<BYTE*>(builder.data()),
      (static_cast<DWORD>(builder.size()) + 1) * sizeof(wchar_t));
}

// wrapper function
bool SetRegValueMULTISZ(ROOT_KEY root,
                        WOW64_OVERRIDE wow64_override,
                        const wchar_t* key_path,
                        const wchar_t* value_name,
                        const std::vector<std::wstring>& values) {
  HANDLE key = INVALID_HANDLE_VALUE;

  if (!OpenRegKey(root, key_path, KEY_SET_VALUE | wow64_override, &key, NULL))
    return false;

  if (!SetRegValueMULTISZ(key, value_name, values)) {
    CloseRegKey(key);
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------
// Enumeration Support
//------------------------------------------------------------------------------

bool QueryRegEnumerationInfo(HANDLE key, ULONG* out_subkey_count) {
  if (!g_initialized && !InitNativeRegApi())
    return false;

  // Use a loop here, to be a little more tolerant of concurrent registry
  // changes.
  NTSTATUS ntstatus = STATUS_UNSUCCESSFUL;
  int tries = 0;
  // Start with sizeof the structure.  It's very common for the variable sized
  // "Class" element to be of length 0.
  KEY_FULL_INFORMATION* key_info = nullptr;
  DWORD size_needed = sizeof(*key_info);
  std::vector<BYTE> buffer(size_needed);
  do {
    buffer.resize(size_needed);
    key_info = reinterpret_cast<KEY_FULL_INFORMATION*>(buffer.data());

    ntstatus = g_nt_query_key(key, KeyFullInformation, key_info, size_needed,
                              &size_needed);
  } while ((ntstatus == STATUS_BUFFER_OVERFLOW ||
            ntstatus == STATUS_BUFFER_TOO_SMALL) &&
           ++tries < kMaxTries);

  if (!NT_SUCCESS(ntstatus))
    return false;

  // Move desired information to out variables.
  *out_subkey_count = key_info->SubKeys;

  return true;
}

bool QueryRegSubkey(HANDLE key,
                    ULONG subkey_index,
                    std::wstring* out_subkey_name) {
  if (!g_initialized && !InitNativeRegApi())
    return false;

  // Use a loop here, to be a little more tolerant of concurrent registry
  // changes.
  NTSTATUS ntstatus = STATUS_UNSUCCESSFUL;
  int tries = 0;
  // Start with sizeof the structure, plus 12 characters.  It's very common for
  // key names to be < 12 characters (without being inefficient as an initial
  // allocation).
  KEY_BASIC_INFORMATION* subkey_info = nullptr;
  DWORD size_needed = sizeof(*subkey_info) + (12 * sizeof(wchar_t));
  std::vector<BYTE> buffer(size_needed);
  do {
    buffer.resize(size_needed);
    subkey_info = reinterpret_cast<KEY_BASIC_INFORMATION*>(buffer.data());

    ntstatus = g_nt_enumerate_key(key, subkey_index, KeyBasicInformation,
                                  subkey_info, size_needed, &size_needed);
  } while ((ntstatus == STATUS_BUFFER_OVERFLOW ||
            ntstatus == STATUS_BUFFER_TOO_SMALL) &&
           ++tries < kMaxTries);

  if (!NT_SUCCESS(ntstatus))
    return false;

  // Move desired information to out variables.
  // NOTE: NameLength is size of Name array in bytes.  Name array is also
  //       NOT null terminated!
  BYTE* name = reinterpret_cast<BYTE*>(subkey_info->Name);
  std::vector<BYTE> content(name, name + subkey_info->NameLength);
  EnsureTerminatedSZ(&content, false);
  out_subkey_name->assign(reinterpret_cast<wchar_t*>(content.data()));

  return true;
}

//------------------------------------------------------------------------------
// Utils
//------------------------------------------------------------------------------

const wchar_t* GetCurrentUserSidString() {
  if (!g_initialized && !InitNativeRegApi())
    return nullptr;

  return g_current_user_sid_string;
}

bool IsCurrentProcWow64() {
  if (!g_initialized && !InitNativeRegApi())
    return false;

  return g_wow64_proc;
}

bool SetTestingOverride(ROOT_KEY root, const std::wstring& new_path) {
  if (!g_initialized && !InitNativeRegApi())
    return false;

  std::wstring sani_new_path = new_path;
  SanitizeSubkeyPath(&sani_new_path);
  if (sani_new_path.length() > g_kRegMaxPathLen)
    return false;

  if (root == HKCU || (root == AUTO && !g_system_install))
    ::wcsncpy(g_HKCU_override, sani_new_path.c_str(), nt::g_kRegMaxPathLen);
  else
    ::wcsncpy(g_HKLM_override, sani_new_path.c_str(), nt::g_kRegMaxPathLen);

  return true;
}

std::wstring GetTestingOverride(ROOT_KEY root) {
  if (!g_initialized && !InitNativeRegApi())
    return std::wstring();

  if (root == HKCU || (root == AUTO && !g_system_install))
    return g_HKCU_override;

  return g_HKLM_override;
}

}  // namespace nt
