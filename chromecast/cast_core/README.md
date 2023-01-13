# Introduction

This directory defines the Cast Web Runtime, an implementation of the
`//components/cast_streaming` Chromecast receiver, for use with the internal
Cast Core library.

# Building

Building of this directory can be done by building the
`chromecast/cast_core:core_runtime_simple` target with the following build
flags:

```
is_debug = true
dcheck_always_on = true
is_component_build = false
symbol_level = 2

is_castos = true
enable_cast_receiver = true
cast_streaming_enable_remoting = true
enable_cast_audio_renderer = false
enable_cast_renderer = false

ffmpeg_branding = "Chrome"
proprietary_codecs = true
```

# Running

Running the Cast Web Runtime on Linux can be done with the following steps:

1. Build the Cast Web Runtime using the above flags
2. Build Cast Core following instructions
[here](https://goto.google.com/cast-core-on-glinux#build).
3. [Run](https://goto.google.com/cast-core-on-glinux#run) the Cast Core and
Platform Service applications built in step 2. Note that if it is your first
time running Cast Core you will need to
[generate certificates](https://goto.google.com/cast-core-on-glinux#certificates).
4. Run the `core_runtime_simple` application built in step 1. The runtime should
immediately be registered with Cast Core.
5. Cast from a Chrome instance running on the local machine. It may take a few
minutes for the Cast Core instance to show up as a valid cast target.
