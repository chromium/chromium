# Credential Management

This component should contain the code that is used for credential management
by Chrome Password Manager, WebView and Chrome in 3P mode.

The component runs in the browser process.

## 3P password manager support

Users can set their default Android autofill service to either Google or a
3P provider. Clank allows users to opt in for 3P autofill experience and have a
consistent UX across Android Autofill and Chrome.

Besides the use-case of using credentials in forms, web pages can request
credentials from browsers using JavaScript and the Credential Management API.
Forwarding the requests to CredMan is the continuation of the browser being a
mediator for 3P requests. The interaction of Blink with browser code is shown on the sequence diagram below.

```
,------.  ,--------------------.            ,--------------. ,-----------. ,-------------.                ,---------------------.      ,--------.
|caller|  |Authentication      |            |Credential    | |ThirdParty | |ThirdParty   |                |Credential           |      |Autofill|
|      |  |CredentialsContainer|            |Manager - mojo| |CredManImpl| |CredManBridge|                |Manager - Jetpack API|      |Service |
`---+--'  `----------+---------'            `---------+----' `-----+-----' `----+--------'                `-----------+---------'      `---+----'
    |     Get()      |                                |            |            |                                     |                    |
    |--------------->|                                |            |            |                                     |                    |
    |                |                                |            |            |                                     |                    |
    |                |get the CredentialManager using |            |            |                                     |                    |
    |                |CredentialManagerProxy          |            |            |                                     |                    |
    |                |------------------------------->|            |            |                                     |                    |
    |                |                                |            |            |                                     |                    |
    |                |      credential_manager        |            |            |                                     |                    |
    |                |<-------------------------------|            |            |                                     |                    |
    |                |                                |            |            |                                     |                    |
    |                |  credential_manager -> Get()   |            |            |                                     |                    |
    |                |------------------------------->|            |            |                                     |                    |
    |                |                                |            |            |                                     |                    |
    |                |                                |   Get()    |            |                                     |                    |
    |                |                                |----------->|            |                                     |                    |
    |                |                                |            |            |                                     |                    |
    |                |                                |            |   get()    |                                     |                    |
    |                |                                |            |----------->|                                     |                    |
    |                |                                |            |            |                                     |                    |
    |                |                                |            |            |              create()               |                    |
    |                |                                |            |            |------------------------------------>|                    |
    |                |                                |            |            |                                     |                    |
    |                |                                |            |            |        getCredentialAsync()         |                    |
    |                |                                |            |            |------------------------------------>|                    |
    |                |                                |            |            |                                     |                    |
    |                |                                |            |            |                                     |Request credentials |
    |                |                                |            |            |                                     |------------------->|
    |                |                                |            |            |                                     |                    |
    |                |                                |            |            |                                     |CredentialResponse  |
    |                |                                |            |            |                                     |<- - - - - - - - - -|
    |                |                                |            |            |                                     |                    |
    |                |                                |            |            |CredentialManagerCallback.onResult() |                    |
    |                |                                |            |            |<- - - - - - - - - - - - - - - - - - |                    |
    |                |                                |            |            |                                     |                    |
    |                |                     OnGetComplete()         |            |                                     |                    |
    |                |<- - - - - - - - - - - - - - - - - - - - - - - - - - - - -|                                     |                    |
    |                |                                |            |            |                                     |                    |
    |Credential info |                                |            |            |                                     |                    |
    |<- - - - - - - -|                                |            |            |                                     |                    |
,---+--.  ,----------+---------.            ,---------+----. ,-----+-----. ,----+--------.                ,-----------+---------.      ,---+----.
|caller|  |Authentication      |            |Credential    | |ThirdParty | |ThirdParty   |                |Credential           |      |Autofill|
|      |  |CredentialsContainer|            |Manager - mojo| |CredManImpl| |CredManBridge|                |Manager - Jetpack API|      |Service |
`------'  `--------------------'            `--------------' `-----------' `-------------'                `---------------------'      `--------'
```
