# Hibernate in Chrome OS

Hibernate, also known as suspend-to-disk, is the process of saving the entire
contents of RAM to disk and shutting the machine down. Upon resume, the kernel
boots fresh, but then with the coordination of usermode diverts to a resume flow
whereby the saved hibernate image is loaded from disk back into RAM, and jumped
into. Since the RAM image itself contains everything (including the kernel,
Chrome, and applications), state restoration is perfect, much like a
suspend/resume cycle. This presents a new point on the power/latency curve where
the user can get the better state-preserving experience of suspend, and the
power consumption of shutdown, at the cost of latency and storage.

For the most part the mechanics of this are handled by daemons below Chrome in
the stack, like powerd, cryptohome, and the hiberman service. However Chrome is
slightly involved in the resume process, as it initiates that diversion into the
load-and-resume flow, and blocks other login activities from occurring while the
resume is in progress. This allows the hibernate image encryption to be tied to
authentication credentials, and keeps resume from contending with other services
starting, which otherwise represents wasted work in the case of a successful
resume.

## Hibernate GN Build Flags

Note: GN Flags are Build time flags

You can get a comprehensive list of all arguments supported by gn by running the
command gn args --list out/some-directory (the directory passed to gn args is
required as gn args will invokes gn gen to generate the build.ninja files).

### enable_hibernate (BUILDFLAG(ENABLE_HIBERNATE))

This build flag enables support for hibernation within Chrome. When set, it
makes a blocking d-bus call upon authentication success to the hiberman daemon
to initiate resume activities.

#### Query Flag
```bash
$ gn args out_<hibernate_overlay>/{Release||Debug} --list-enable_hibernate
```

#### Enable Flag
```bash
$ gn args out_<hibernate_overlay>/{Release||Debug}
$ Editor will open, add enable_hiberate=true, save and exit
```
