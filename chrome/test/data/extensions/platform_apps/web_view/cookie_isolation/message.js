// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This namespace automates the process of running taks which require
// communication in between different |window|'s. Communication is through
// posting "Messaging.Message" objects to different windows. The messages are
// handled by the "Messaging.Handler" which will in turn dispatch it to the
// corresponding agent. Agents communicate in pairs. When an agent requires a
// task to be done by another agent in a different window, a message is posted
// and on the other side, it is dispatched to the corresponding agent to do the
// task.
Messaging = {};

// A simple message between |source| and |destination| agents.
Messaging.Message = function (source, destination, content) {
  this.source = source;
  this.destination  = destination;
  this.content = content;
};

// A Singleton class which listens to |window.onmessage| event and upon
// receiving a message, dispatches the message to the corresponding agent. The
// agents are registered using |Messaging.Handler.addAgent|.
Messaging.GetHandler = function() {
  if (arguments.callee._singletonInstance) {
    return arguments.callee._singletonInstance;
  }
  arguments.callee._singletonInstance = this;
  var agents = {};
  this.hasAgent = function(agentName) {
    return !!agents[agentName];
  };
  this.addAgent = function(agent) {
    agents[agent.getName()] = agent;
  };
  this.sendMessage = function(message, portTo) {
    console.log('Sending message from "' + message.source + '" to "' +
        message.destination + '".');
    portTo.postMessage(message, '*');
  };
  var handler = this;
  function onMessage(msg) {
    var message = msg.data;
    var from = msg.source;
    console.log('Received message from "'+ message.source + '" to "' +
        message.destination + '".');
    if (handler.hasAgent(message.destination)) {
      console.log('Dispatching message to agent: ' + message.destination)
          agents[message.destination].receive(message, from);
    } else {
      console.log('Unknown agent "' + message.destination  + '".');
    }
  }
  window.onmessage = onMessage;
  return handler;
};

// An agent is a point of communication within a |window|. Agent's communicate
// with each other to make requests. The request is identified in the
// |Messaging.Message.type| field. Each agent must have a handler function
// registered for each type of incoming message it receives.
Messaging.Agent = function(name) {
  var agentName = name;
  this.getName = function() {
    return agentName;
  };
  var types = {};
  this.handlesType = function(type) {
    return !!types[type];
  };
  this.addTask = function(type, handler) {
    types[type] = handler;
  };
  this.getHandler = function(type) {
    return types[type];
  };
};

// The message handler function for each agent. |portFrom| is the
// window/contentWindow of the origin of the message.
Messaging.Agent.prototype.receive = function(message, portFrom) {
  if (this.handlesType(message.content.type)) {
    console.log('Agent "' + this.getName() + '" will handle the message type"' +
        message.content.type + '".');
    this.getHandler(message.content.type)(message, portFrom);
  } else {
    console.log('Agent "' + this.getName() + '" cannot handle message type "' +
        message.content.type + '".');
  }
};
