# Introduction

This directory defines the Cast Web Runtime, an implementation of the
`//components/cast_streaming` Chromecast receiver, for use with the internal
Cast Core library.

# Local Testing

### The below instructions are intended for Googlers only!

## Build

Build flags for Cast Web Runtime can be found in `//chromecast/build/args/product/`:
 - atv* configs target Android TV
 - linuxtv* configs target Linux-based TVs.

Building of this directory can be done by building the `chromecast/cast_core:core_runtime_simple` target.


## Run

Running the Cast Web Runtime on Linux can be done with the following steps:

1. Build the Cast Web Runtime using the above flags
2. Build Cast Core following instructions
[here](https://goto.google.com/cast-core-on-glinux#build).
3. [Run](https://goto.google.com/cast-core-on-glinux#run) the Cast Core and
Platform Service applications built in step 2. Note that if it is your first
time running Cast Core you will need to
[generate certificates](https://goto.google.com/cast-core-on-glinux#certificates)
for device type `tv`.
4. Cast from a Chrome instance running on the local machine (either an official
Chrome release or a locally built Chromium instance). It may take a few minutes
for the Cast Core instance to show up as a valid cast target.

# Troubleshooting

## Common issues and their resolutions:

When running Cast Core, error
`Unable to load application config from file <path>/app.conf because the file does not exist or could not be read`:
This occurs when the certificates were not generated correctly. Ensure they were
generated for device type `tv`.

When starting the Cast Web Runtime, hitting
[this DCHECK](https://source.chromium.org/chromium/chromium/src/+/main:components/metrics/metrics_state_manager.cc;l=414):
This occurs when flags are not set correctly. Ensure you are using the correct
config file for running on Linux.

When starting Cast Core, you hit
[this DCHECK](https://goto.google.com/castcoreportfailure): This occurs when the
gRPC port is already is use. Kill the existing process using the following
commands:

```
user$ netstat -ltnp | grep -w "10101"
(Not all processes could be identified, non-owned process info
 will not be shown, you would have to be root to see it all.)
tcp6       0      0 :::10101                :::*                    LISTEN      <process id>/<path>
user$ kill -9 <process id>
```

Then re-run Cast Core.
