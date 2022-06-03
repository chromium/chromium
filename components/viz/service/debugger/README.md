# //components/viz/service/debugger

## Viz Remote Visual Debugger

The Viz Remote Debugger is a debug only connection that allows an active chromium instance to send graphical and text debug information to a remote client for display.

### Motivation
[Visual Debugger one-pager](https://docs.google.com/document/d/1s2OLZcUrUxXRxOD8nR4giNRnCQZcdtXBp4DUI_Fb2GU/edit?usp=sharing).


### Usage
Macros constitute the full Viz Debugger logging API  (as far as a nominal chromium developer is concerned).

These macros tend to take the basic form below:

DBG_DRAW_RECT(anno, rect);

This macro will log a rect to the viz debugger for this frame.
anno - short for Annotation. This must be a string literal. The remote can filter on anno (as well as file and function).
rect - a (gfx::) rect. Expected dimensions is in pixels.

Unlike a debugging session printfs these logging macros can be remain the chromium source. This allows developers to use these debug logs in future sessions and other share them with other developers.


The full list of these macros can be found in VizDebugger.h
At present, Visual debugging is currently limited to the VizCompositor thread.


### Operation
The debugging macros feed information into the VizDebugger static instance. At the end of each frame this cached information is fed upstream and eventually reaches the remote client.

[Viz Debugger Communication](https://docs.google.com/drawings/d/11zqorcaRuyGx7W2AdL-7hSQJG0wknm0-RDkZSyobmy4/edit?usp=sharing)


### Performance
The logging performance of this system has several variants depending on the configuration.

* Disabled at Compile time

 Zero overhead with the exception of mutable side effects in the creation of input variables.

* Disabled at Runtime

 Every log call must check the debugging instance. This case has been optimized to be one memory read instruction followed by one predictable branch.

* Disabled by Source Filter

 Must read the local static data to determine if this specific logging is enabled. This is on the order of tens of instructions.

* Enabled

 Submits data into VizDebugger buffers. On the order of 100 instructions with the exception of when these buffers need to expand.


Additional performance overhead of this system comes from when the collected data is serialized and sent upstream. Unlike the logging submission overhead, this overhead is very easy to track and currently is not consider to be a concern.


### Security

For official builds all visual debug macros are compiled out and the VizDebugger is reduced to a selective few method stubs. This means there are no security concerns for this system with exception of mutable side effects in the creation of input variables to the logging macros. These concerns for mutable side effects already exist for any other code; so no special attention is warranted for these macros.

For non-official (debug) builds the dev-tools remote debugging port command line argument must be provided for the viz debugger to collect and stream data. Thus the security of this new system is identical to that of the remote dev tools for the case of debugging builds.
