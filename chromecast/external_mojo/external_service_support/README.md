# External Mojo service support

This directory contains utilities to ease development of Mojo services that run
in processes outside of Chromium/cast_shell. The simplest model is to create a
subclass of 'ServiceProcess' (and implement 'ServiceProcess::Create()' that
returns an instance of that subclass), and link with 'standalone_service_main'
to create the executable. In your 'ServiceProcess' implementation, you can bind
to Mojo interfaces using the provided 'Connector' pointer, and/or register your
own Mojo services for use by other processes.

The 'ChromiumServiceWrapper' class is intended to allow Mojo services that were
intended to be embedded into cast_shell (or other ServiceManager embedder) to
be moved into a completely separate process. It forwards 'BindInterface()'
calls to the 'service_manager::Service' API. You can use
'CreateChromiumServiceReceiver()' to create a
'mojo::PendingReceiver<service_manager::Service>' to emulate the normal service
creation flow; the 'service_manager::Service::OnStart()' method will be called
automatically.

The 'standalone_mojo_broker' is intended for use on platforms where there is no
cast_shell running; this allows Mojo services outside of cast_shell to
communicate with each other without any Chromium embedder on the system.
