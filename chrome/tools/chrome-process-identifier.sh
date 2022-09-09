#!/bin/bash

# Copyright 2010 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This utility finds the different processes in a running instance of Chrome.
# It then attempts to identify their types (e.g. browser, extension, plugin,
# zygote, renderer). It also prints out information on whether a sandbox is
# active and what type of sandbox has been identified.

# This script is likely to only work on Linux or systems that closely mimick
# Linux's /proc filesystem.
[ -x /proc/self/exe ] || {
  echo "This script cannot be run on your system" >&2
  exit 1
}

# Find the browser's process id. If there are multiple active instances of
# Chrome, the caller can provide a pid on the command line. The provided pid
# must match a process in the browser's process hierarchy. When using the
# zygote inside of the setuid sandbox, renderers are in a process tree separate
# from the browser process. You cannot use any of their pids.
# If no pid is provided on the command line, the script will randomly pick
# one of the running instances.
if [ $# -eq 0 ]; then
  pid=$(ls -l /proc/*/exe 2>/dev/null |
        sed '/\/chrome\( .deleted.\)\?$/s,.*/proc/\([^/]*\)/exe.*,\1,;t;d' |
        while read p; do
          xargs -0 </proc/$p/cmdline 2>/dev/null|grep -q -- --type= && continue
          echo "$p"
          break
        done)
else
  pid="$1"
fi
ls -l "/proc/$pid/exe" 2>/dev/null|egrep -q '/chrome( .deleted.)?$' || {
  echo "Cannot find any running instance of Chrome" >&2; exit 1; }
while :; do
  ppid="$(ps h --format ppid --pid "$pid" 2>/dev/null)"
  [ -n "$ppid" ] || {
    echo "Cannot find any running instance of Chrome" >&2; exit 1; }
  ls -l "/proc/$ppid/exe" 2>/dev/null|egrep -q '/chrome( .deleted.)?$' &&
    pid="$ppid" || break
done
xargs -0 </proc/$p/cmdline 2>/dev/null|grep -q -- --type= && {
  echo "Cannot find any running instance of Chrome" >&2; exit 1; }

# Iterate over child processes and try to identify them
identify() {
  local child cmd foundzygote plugin seccomp type
  foundzygote=0
  for child in $(ps h --format pid --ppid $1); do
    cmd="$(xargs -0 </proc/$child/cmdline|sed 's/ -/\n-/g')" 2>/dev/null
    type="$(echo "$cmd" | sed 's/--type=//;t1;d;:1;q')"
    case $type in
      '')
        echo "Process $child is part of the browser"
        identify "$child"
        ;;
      extension)
        echo "Process $child is an extension"
        ;;
      plugin)
        plugin="$(echo "$cmd" |
                 sed 's/--plugin-path=//;t1;d;:1
                      s,.*/lib,,;s,.*/npwrapper[.]lib,,;s,^np,,;s,[.]so$,,;q')"
        echo "Process $child is a \"$plugin\" plugin"
        identify "$child"
        ;;
      renderer|worker|gpu-process)
        # The seccomp sandbox has exactly one child process that has no other
        # threads. This is the trusted helper process.
        seccomp="$(ps h --format pid --ppid $child|xargs)"
        if [ -d /proc/$child/cwd/. ]; then
          if [ $(echo "$seccomp" | wc -w) -eq 1 ] &&
             [ $(ls /proc/$seccomp/task 2>/dev/null | wc -w) -eq 1 ] &&
             ls -l /proc/$seccomp/exe 2>/dev/null |
               egrep -q '/chrome( .deleted.)?$'; then
            echo "Process $child is a sandboxed $type (seccomp helper:" \
                 "$seccomp)"
          else
            echo "Process $child is a $type"
            identify "$child"
          fi
        else
          if [ $(echo "$seccomp" | wc -w) -eq 1 ]; then
            echo "Process $child is a setuid sandboxed $type (seccomp" \
                 "helper: $seccomp)"
          else
            echo "Process $child is a $type; setuid sandbox is active"
            identify "$child"
          fi
        fi
        ;;
      zygote)
        foundzygote=1
        echo "Process $child is the zygote"
        identify "$child"
        ;;
      *)
        echo "Process $child is of unknown type \"$type\""
        identify "$child"
        ;;
    esac
  done
  return $foundzygote
}

cmpcmdline() {
  # Checks that the command line arguments for pid $1 are a superset of the
  # commandline arguments for pid $2.
  # Any additional function arguments $3, $4, ... list options that should
  # be ignored for the purpose of this comparison.
  local pida="$1"
  local pidb="$2"
  shift; shift
  local super=("$@" $(xargs -0 </proc/"$pida"/cmdline)) 2>/dev/null
  local sub=($(xargs -0 </proc/"$pidb"/cmdline)) 2>/dev/null
  local i j
  [ ${#sub[*]} -eq 0 -o ${#super[*]} -eq 0 ] && return 1
  for i in $(seq 0 $((${#sub[*]}-1))); do
    for j in $(seq 0 $((${#super[*]}-1))); do
      [ "x${sub[$i]}" = "x${super[$j]}" ] && continue 2
    done
    return 1
  done
  return 0
}


echo "The browser's main pid is: $pid"
if identify "$pid"; then
  # The zygote can make it difficult to locate renderers, as the setuid
  # sandbox causes it to be reparented to "init". When this happens, we can
  # no longer associate it with the browser with 100% certainty. We make a
  # best effort by comparing command line strings.
  for i in $(ps h --format pid --ppid 1); do
    if cmpcmdline "$pid" "$i" "--type=zygote"; then
      echo -n "Process $i is the zygote"
      [ -d /proc/$i/cwd/. ] || echo -n "; setuid sandbox is active"
      echo
      identify "$i"
    fi
  done
fi
